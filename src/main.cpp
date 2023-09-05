/* my attempt at creating a shell in C++ with some C */

#include <iostream>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <sys/wait.h>
#include <pwd.h>

#define ESC 27

namespace config
{
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
	}USR_INFO;

	typedef struct {
		std::string path;
		std::string home;
		std::string prompt;
		std::vector<std::string> usr_vars;
	}VARS;

	USR_INFO *usr_info;
	VARS vars;

	/* gets data about the user from pwd */
	void get_info (bool first)
	{
		struct passwd *pwd;

		if (first == true)
			config::usr_info = new USR_INFO[sizeof(*config::usr_info)];

		/* get uid */
		config::usr_info->uid = getuid();

		/* getting user information */
		pwd = getpwuid(config::usr_info->uid);

		/* setting user information */
		config::usr_info->username = strndup(pwd->pw_name, 256);
		config::usr_info->password = strndup(pwd->pw_passwd, 256);
		config::usr_info->home = strndup(pwd->pw_dir, 256);
	}

	/* makes the config file path from the username and the home dir */
	void make_path (void)
	{
		constexpr char *conf = (char*) "/.cppshell.conf";
		char *path = new char [sizeof(config::usr_info->home) + strlen(conf)];

		strncat(config::usr_info->home, conf, strlen(conf));

		config::vars.path = path;

		delete[] path;
	}

	/* sets the prompt from the config */
	void set_prompt (void)
	{
		std::ifstream file;
		std::string line, prompt, tmp;

		file.open(config::vars.path, std::ios::out);

		/* if no file use the default prompt */
		if (!file.is_open()) {
			config::vars.prompt = "# ";
			return;
		}

		/* read until it finds the string PROMPT */
		for (int i = 0; std::getline(file, line); i++) {
			/* get first word of string */
			std::istringstream iss(line);
			std::getline(iss, tmp, ' ');

			/* get second and scan it */
			if (tmp == "PROMPT")
				std::getline(iss, tmp, ' ');
		}

		file.close();
		config::vars.prompt = prompt;
	}

	/* sets up variables from the config file */
	void setup (bool first)
	{
		config::get_info(first);
		config::make_path();
		config::set_prompt();
	}
}

namespace cmd
{
	void prev_cmd (char **argv)
	{

	}

	void quit (char **argv)
	{
		exit(0);
	}

	/* implementation of the cd command */
	void cd (char **argv)
	{
		if (chdir(argv[1]) != 0)
			std::cerr << argv[1] << ": No such directory" << std::endl;
	}
}

namespace cli
{
	struct SHELL_CMDS {
		const char *cmd;
		void (*fn)(char **argv);
	};

	/* the built in shell commands 
	 * Ones at top are 'most used' */
	struct SHELL_CMDS shell_cmds[] = {
		{ "cd", cmd::cd },
		{ "!!", cmd::prev_cmd },
		{ "export", nullptr },
		{ "exit", cmd::quit },
		{ "quit", cmd::quit },
	};

		/* returns index of the matching command in shell_cmds struct */
	static ssize_t is_built_in (char *cmd)
	{
		for (size_t i = 0; shell_cmds[i].cmd != nullptr; i++)
			if (!strncmp(shell_cmds[i].cmd, cmd, strlen(shell_cmds[i].cmd)))
				return i;

		return -1;
	}

	/* parses the cmd line args passed to the shell program */
	// TODO create some args and bools for them 
	void parse_args (char *argv[])
	{
		for (size_t i = 0; argv[i] != nullptr; i++)
			if (argv[i][0] == '-' && argv[i][1] != (char) 0)
				for (size_t k = 1; argv[i][k] != (char) 0; k++)
					switch (argv[i][k]) {
						case 'r':
							break;
					}
	}

	/* runs a command */
	int run_cmd (int argc, char **argv)
	{
		int ret, status;
		pid_t pid, ppid;

		if (argv[0] == nullptr)
			return 0; // Do nothing, if nothing is passed

		/* check for shell commands */
		else if ((ret = is_built_in(strdup(argv[0]))) != -1)
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

		return 0;
	}

	/* the entry point for the cli */
	void entry (void)
	{
		constexpr size_t MAX_LEN = 256;
		int argc;
		char *argv[MAX_LEN];
		std::string input, tmp;

		config::setup(true);

		/* the main loop of the program */
		while (true) {
			std::cout << config::vars.prompt;
			std::getline(std::cin, input);

			/* splitting string into command and argv */
			std::istringstream iss(input);

			for (argc = 0; std::getline(iss, tmp, ' '); argc++) {
				if (argc < MAX_LEN)
					argv[argc] = strndup(tmp.c_str(), strlen(tmp.c_str()));
				else {
					std::cerr << "ERROR: Command too long!" << std::endl;
					argv[0] = nullptr;
					break;
				}
			}

			cli::run_cmd(argc, argv);

			/* clear the values */
			for (size_t i = 0; i < argc; i++)
				argv[i] = nullptr;
		}

		cmd::quit(nullptr);
	}
}

int main (int argc, char *argv[])
{
	(argc < 2) ? cli::entry() : cli::parse_args(argv);
	return 0;
}
