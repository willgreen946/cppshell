/* my attempt at creating a shell in C++ with some C */

#include <iostream>
#include <sstream>

/* C libs */
#include <cstring>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#include <pwd.h>

namespace config
{
	std::string vars[256];
}

namespace cli
{
	void exit (char *argv[]);

	/* holds info about the user */
	typedef struct {
		char *username; // username
		char *password; // encrypted password
		uid_t uid;  // user uid 
		gid_t gid; // user gid
		time_t change; // last password change 
		char *pw_class; // user access class
		char *gecos; // honeywell login info
		char *def_shell; // default shell
		char *home; // home directory
		time_t expire; // account expiration 
		int fields; // internal fields	
	}INFO;

	INFO *info;

	struct SHELL_CMDS {
		const char *cmd;
		void (*fn)(char *argv[]);
	};

	/* the built in shell commands */
	struct SHELL_CMDS shell_cmds[] = {
		{ "export", nullptr },
		{ "exit", cli::exit },
		{ "export", nullptr },
		{ "export", nullptr },
	};

	/* gets data about the user */
	static void get_info (bool first)
	{
		struct passwd *pwd;

		if (first)
			info = new INFO[sizeof(*info)];

		/* get uid */
		info->uid = getuid();

		/* getting user information */
		pwd = getpwuid(info->uid);

		/* setting user information */
		info->username = strndup(pwd->pw_name, strlen(pwd->pw_name));
		info->password = strndup(pwd->pw_passwd, strlen(pwd->pw_passwd));
		info->home = strndup(pwd->pw_dir, strlen(pwd->pw_dir));
	}

	void exit (char *argv[])
	{
		delete[] info;
		exit(0);
	}

	/* returns index of the matching command in shell_cmds struct */
	static ssize_t is_built_in (char *cmd)
	{
		for (size_t i = 0; shell_cmds[i].cmd != nullptr; i++)
			if (!strncmp(shell_cmds[i].cmd, cmd, strlen(shell_cmds[i].cmd)))
				return i;

		return -1;
	}

	/* the entry point for the cli */
	void entry (void)
	{
		char *argv[256];
		int argc;
		int status;
		ssize_t ret;
		pid_t pid;
		pid_t ppid;
		std::string input;
		std::string tmp;

		cli::get_info(true);

		/* the main loop of the program */
		while (true) {
			std::cout << "[" << info->username << "]" << "# ";
			std::getline(std::cin, input);

			/* splitting string into command and argv */
			std::istringstream iss(input);

			for (argc = 0; std::getline(iss, tmp, ' '); argc++)
				argv[argc] = strndup(tmp.c_str(), strlen(tmp.c_str()));

			/* check for shell commands */
			if ((ret = is_built_in(argv[0])) != -1)
				shell_cmds[ret].fn(argv);

			/* if argv[0] is not a shell command then execute a binary */
			else {
				/* fork the process and execute the command */
				pid = fork();

				if (pid == 0) {
					execvp(argv[0], argv);
					std::cerr << argv[0] << ": Unkown Command" << std::endl;
				}

				else
					do {
						ppid = wait(&status);
						if (ppid != pid) break;
					} while (ppid != pid);
			}

			/* clear the values */
			input.clear();
			tmp.clear();

			for (size_t i = 0; i < argc; i++)
				argv[i] = nullptr;
		}

		exit(nullptr);
	}
}

int main (int argc, char *argv[])
{
	if (argc < 2) cli::entry();
	return 0;
}
