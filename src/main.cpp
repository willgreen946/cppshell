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

namespace sys 
{
	/* holds info about the user */
	typedef struct {
		char *username; // username
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

	USR_INFO *usr_info;

	/* gets data about the user from pwd */
	void get_info (bool first)
	{

		struct passwd *pwd;

		if (first == true)
			sys::usr_info = new USR_INFO[sizeof(*sys::usr_info)];

		/* get uid */
		sys::usr_info->uid = getuid();

		/* getting user information */
		pwd = getpwuid(sys::usr_info->uid);

		/* setting user information */
		sys::usr_info->username = strndup(pwd->pw_name, 256);
		sys::usr_info->home = strndup(pwd->pw_dir, 256);
	}

	/* returns hostname of machine */
	std::string get_host (void)
	{
		std::string hostname;
		char *name = new char[256];

		gethostname(name, 256);
		hostname = name;

		delete[] name;
		return hostname;
	}
}

namespace config
{
	void set_prompt (std::string& str);

	struct VAR_MAP {
		std::string var;
		void (*fn)(std::string& str);
	};

	constexpr int var_map_max = 2;
	struct VAR_MAP var_map[var_map_max] = {
		{ "PS1", set_prompt }, 
		{ "PROMPT", set_prompt }, 
	};

	typedef struct {
		std::string path;
		std::string home;
		std::string prompt;
		std::vector<std::string> usr_vars;
	}VARS;

	VARS vars;

	/* makes the config file path from the username and the home dir */
	void make_path (void)
	{
		constexpr char *conf = (char*) "/.cppshell.conf";
		char *path = new char [sizeof(sys::usr_info->home) + strlen(conf)];

		/* creating the path as a single string */
		strncat(path, sys::usr_info->home, 256);
		strncat(path, conf, 256);

		config::vars.path = path;

		delete[] path;
	}

	/* sets the prompt from the config */
	void set_prompt (std::string& str)
	{
		std::string prompt;
		std::vector<std::string> var;

		/* look for variables in the prompt definition */
		for (size_t i = 0, k = 0; i < str.length(); i++) {
			if (str[i] == '@' && i+1 != str.length())
				switch (str[i+1]) {
					case 'h':
						var.push_back(sys::get_host());
						break;
					case 'u':
						break;
					default:
						std::cout << "Unknown option " << str[i] << str[i+1] << std::endl;
						break;
				}
		}

		prompt = str;
		config::vars.prompt = prompt;
	}

	int parse (std::string& var, std::string& arg)
	{
		/* check if it's a built in variable */ 
		for (int i = 0; i < config::var_map_max; i++)
			if (config::var_map[i].var == var)
				config::var_map[i].fn(arg);

		return 0;
	}

	void read (void)
	{
		bool usedefs_f = false;
		std::ifstream file;
		std::string line;
		std::vector<std::string> tmp;

		file.open(config::vars.path, std::ios::out);

		/* if no file is found or cant be opened */
		if (!file.is_open()) {
			usedefs_f = true;
			return;
		}

		/* read until the end and pass it off to the parser */
		for (size_t i = 0; std::getline(file, line); i++) {
			std::istringstream iss(line);

			/* get the words */
			for (std::string line; std::getline(iss, line, '=');)
				tmp.push_back(line);

			config::parse(tmp[0], tmp[1]);
		}

		file.close();
	}

	/* sets up variables from the config file */
	void setup (bool first)
	{
		sys::get_info(first);
		config::make_path();
		config::read();
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
		if (argv[1] == nullptr)
			chdir(sys::usr_info->home);
		else if (chdir(argv[1]) != 0)
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
	constexpr size_t max = 5;
	struct SHELL_CMDS shell_cmds[max] = {
		{ "cd", cmd::cd },
		{ "!!", cmd::prev_cmd },
		{ "export", nullptr },
		{ "exit", cmd::quit },
		{ "quit", cmd::quit },
	};

	/* returns index of the matching command in shell_cmds struct */
	ssize_t is_built_in (char *cmd)
	{
		// TODO seg faulting ??
		/*for (size_t i = 0; shell_cmds[i].cmd != nullptr; i++)
			if (!strncmp(shell_cmds[i].cmd, cmd, strlen(shell_cmds[i].cmd)))
				return i;*/

		for (size_t i = 0; i < max; i++)
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
