/* my attempt at creating a shell in C++ with some C */

#include <iostream>
#include <vector>
#include <map>
#include <sstream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <sys/wait.h>
#include <pwd.h>

#define ESC 27

namespace cli
{
	int run_cmd (int argc, char **argv);
}

namespace sys 
{
	/* holds info about the user */
	typedef struct {
		char *username; // username of caller
		char *hostname; // hostname of machine
		uid_t uid;  // user uid 
		gid_t gid; // user gid
		char *pw_class; // user access class
		char *home; // home directory
	}USR_INFO;

	USR_INFO *usr_info;

	/* gets data about the user from pwd */
	void get_info (void)
	{
		struct passwd *pwd;
		char *tmp = new char[1024];

		sys::usr_info = new USR_INFO[sizeof(*sys::usr_info)];

		/* get uid */
		sys::usr_info->uid = getuid();

		/* getting user information */
		pwd = getpwuid(sys::usr_info->uid);

		/* setting usename and home directory */
		sys::usr_info->username = strndup(pwd->pw_name, 256);
		sys::usr_info->home = strndup(pwd->pw_dir, 256);

		/* setting hostname of machine */
		gethostname(tmp, 256);
		sys::usr_info->hostname = tmp;

		delete[] tmp;
	}
}

namespace config
{
	void set_prompt (void);

	/* this is where all the variables are stored
	 * some rules:
	 * all built in variables to be uppercase
	 * the first string element is the variable name
	 * the second string is the value of that variable */
	std::map<std::string, std::string> varmap;

	/* holds the string for the PS1 */
	std::string prompt_str;

	/* looks up a variable */
	std::string get_var (std::string variable)
	{
		if (varmap.find(variable) != varmap.end())
			return config::varmap[variable];
		else
			return " "; // Return whitespace

		return " ";
	}

	/* makes the config file path from the username and the home dir */
	void make_path (void)
	{
		constexpr char *conf = (char*) "/.cppshell.conf";
		char *path = new char [sizeof(sys::usr_info->home) + strlen(conf)];

		/* creating the path as a single string */
		strncat(path, sys::usr_info->home, 256);
		strncat(path, conf, 256);

		config::varmap["CONFIG"] = path;

		delete[] path;
	}

	/* this returns a string to an @ variable */
	std::string get_id_var (char ch)
	{
		std::string value;

		/* look for variables in the prompt definition */
				switch (ch) {
					case 'h':
						value = config::varmap["HOST"];
						break;
					case 'u':
						value = config::varmap["USER"];
						break;
					case 't':
						value = config::varmap["TIME"];
						break;
					case 'p':
						value = config::varmap["PWD"];
						break;
					default:
						std::cout << "Unknown option " << '@' << ch << '\n';
						return " "; // Return whitespace instead of null
				}	

		return value;
	}

	/* sets the prompt from the config */
	void set_prompt (void)
	{
		std::string str = config::varmap["PS1"];

		char *prompt = new char[sizeof(str)];

		if (str[0] == (char) 0) {
			config::varmap["PS1"] = "# ";
			return;
		}

		/* concatonate prompt and looks for vars */
		for (size_t i = 0; i < str.length(); i++) {
			if (str[i] == '@' && i++ != str.length())
				strcat(prompt, config::get_id_var(str[i]).c_str());
			else if (str[i] == '\\' && i+1 != str.length())
				strncat(prompt, "@", 1);
			else strncat(prompt, &str[i], 1);
		}

		config::prompt_str = str; 
		config::varmap["PS1"] = prompt;

		/* reset values and free memory */
		memset(prompt, 0, sizeof((char*)prompt));
		delete[] prompt;
	}

	/* decide what to do with config args */
	int parse (std::string& var, std::string& arg)
	{
		/* check for comments */
		if (arg[0] == '#')
			return 0;

		config::varmap[var] = arg;
		return 0;
	}

	void read (void)
	{
		std::ifstream file;
		std::string line, fw, args;

		file.open(config::varmap["CONFIG"], std::ios::out);

		/* if no file is found or cant be opened do nothing */
		if (!file.is_open())
			return;

		/* read until the end and pass it off to the parser */
		for (size_t i = 0; std::getline(file, line); i++) {
			std::istringstream iss(line);

			for (int ln = 0; std::getline(iss, line, '='); ln++) {
				if (ln == 0) fw = line;
				else args = line;
			}
			
			std::cout << fw << '\n' << args << '\n';
			config::parse(fw, args);
		}

		file.close();
	}

	/* sets up variables from the config file */
	void setup (void)
	{
		char wd[256];

		/* get info about the user */
		sys::get_info();

		/* get working directory */
		if (getcwd(wd, sizeof(wd)) == nullptr)
			std::cerr << "ERROR: with getcwd" << '\n';

		/* setup some default variables */
		config::varmap["HOME"] = sys::usr_info->home; 
		config::varmap["HOST"] = sys::usr_info->hostname; 
		config::varmap["USER"] = sys::usr_info->username; 
		config::varmap["PWD"] = wd;
		config::varmap["SHELL"] = "cppshell";
		config::varmap["PS1"] = "# ";
		config::varmap["CONFIG"] = "/";

		/* attempt to read the config where ^ may change */
		config::make_path();
		config::read();
	}
}

namespace cmd
{
	class history {
		public:
			std::string prev;
			std::vector<std::string> history_store;

			std::string mono_str (char *str[])
			{
				std::string mono;

				for (size_t i = 0; str[i] != nullptr; i++) {
					if (i > 0) {
						mono += " ";
						mono += str[i];
					}

					else mono += str[i];
				}

				return mono;
			}

			/* writes the history the history vector */
			void write (char *argv[])
			{
				std::string str = cmd::history::mono_str(argv);
				history_store.push_back(str);
				//std::cout << "his = " << *history_store.rbegin() << '\n';
				//std::cout << "str = " << str << '\n';
			}
	};

	cmd::history history;

	/* prints text to console passed at command line */
	void echo (char **argv)
	{
		char *tmp = new char[sizeof(argv)];

		for (size_t i = 1; argv[i] != nullptr; i++) {
			/* check for variables */
			if (argv[i][0] == '$' && argv[i][1] != (char) 0) {
				for (size_t k = 1; argv[i][k] != (char) 0; k++)
					strncat(tmp, &argv[i][k], 1);

				/* print out variable */
				std::cout << config::get_var(tmp) << " ";

				/* set all tmp chars to \0 */
				for (size_t k = 0; tmp[k] != (char) 0; k++)
			  	tmp[k] = (char) 0;

				i++;
			} else std::cout << argv[i] << " ";
		}

		std::cout << '\n';

		/* set all tmp chars to \0 */
		for (size_t i = 0; tmp[i] != (char) 0; i++)
			tmp[i] = (char) 0;

		delete[] tmp;
	}

	/* runs the last command entered */
	// TODO
	void prev_cmd (char **argv)
	{
		int argc;
		char *arg[256];
		//std::string str = cmd::history::*history_store.rbegin();
		std::string tmp;
		//std::istringstream iss(str);

		/*for (int i = 0; i < 1; i++, argc++) {
			std::getline(iss, tmp, ' ');
			if (i == 0) cmd_str = tmp;
		}*/

		//cli::run_cmd(argc, str.c_str());
	}

	void quit (char **argv)
	{
		exit(0);
	}

	void export_cmd (char **argv)
	{

	}

	/* implementation of the cd command */
	void cd (char **argv)
	{
		if (argv[1] == nullptr)
			chdir(config::varmap["HOME"].c_str());
		else if (chdir(argv[1]) != 0)
			std::cerr << argv[1] << ": No such directory" << '\n';
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
	constexpr size_t max = 6;
	struct SHELL_CMDS shell_cmds[max] = {
		{ "cd", cmd::cd },
		{ "echo", cmd::echo },
		{ "!!", cmd::prev_cmd },
		{ "export", cmd::export_cmd },
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
				std::cerr << argv[0] << ": Unkown Command" << '\n';
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
		char wd[256];
		std::string input, tmp;
		
		config::setup();
		config::set_prompt();

		/* the main loop of the program */
		while (true) {
			std::cout << config::varmap["PS1"];
			std::getline(std::cin, input);

			/* splitting string into command and argv */
			std::istringstream iss(input);

			for (argc = 0; std::getline(iss, tmp, ' '); argc++) {
				if (argc < MAX_LEN)
					argv[argc] = strndup(tmp.c_str(), strlen(tmp.c_str()));
				else {
					std::cerr << "ERROR: Command too long!" << '\n';
					argv[0] = nullptr;
					break;
				}
			}

			cli::run_cmd(argc, argv);

			/* set working directory */
			getcwd(wd, sizeof(wd));
			config::varmap["PWD"] = wd;

			/* save history in memory */
			cmd::history.write(argv);

			/* clear the values */
			for (size_t i = 0; i < argc; i++)
				argv[i] = nullptr;

			memset(wd, 0, sizeof((char*)wd));
		}
		
		cmd::quit(nullptr);
	}
}

int main (int argc, char *argv[])
{
	(argc < 2) ? cli::entry() : cli::parse_args(argv);
	return 0;
}
