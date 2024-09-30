// SPDX-License-Identifier: BSD-3-Clause

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cmd.h"
#include "utils.h"

#define READ		0
#define WRITE		1

/**
 * Internal change-directory command.
 */
static bool shell_cd(word_t *dir)
{
	/* TODO: Execute cd. */
	int ret;
	char *new_dir;

	new_dir = get_word(dir);
	ret = chdir(new_dir);

	free(new_dir);

	if (!ret)
		return 0;
	return 1;
}

/**
 * Internal exit/quit command.
 */
static int shell_exit(void)
{
	/* TODO: Execute exit/quit. */

	return SHELL_EXIT; /* TODO: Replace with actual exit code. */
}

static void redirect_input(int flags, char *name)
{
	int fd, rc;

	fd = open(name, flags, 0644);
	DIE(fd < 0, "open");

	rc = dup2(fd, STDIN_FILENO);
	DIE(rc < 0, "dup2");

	close(fd);
}

static void redirect_output(int flags, char *name)
{
	int fd, rc;

	if (flags)
		fd = open(name, O_APPEND | O_CREAT | O_RDWR, 0644);
	else
		fd = open(name, O_TRUNC | O_CREAT | O_WRONLY, 0644);
	DIE(fd < 0, "open");

	rc = dup2(fd, STDOUT_FILENO);
	DIE(rc < 0, "dup2");

	close(fd);
}

static void redirect_error(int flags, char *name)
{
	int fd, rc;

	if (flags)
		fd = open(name, O_APPEND | O_CREAT | O_RDWR, 0644);
	else
		fd = open(name, O_TRUNC | O_CREAT | O_WRONLY, 0644);
	DIE(fd < 0, "open");

	rc = dup2(fd, STDERR_FILENO);
	DIE(rc < 0, "dup2");

	close(fd);
}

static void free_argv(char **argv, int argc)
{
	for (int i = 0; i < argc; i++)
		free(argv[i]);
	free(argv);
}
/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	/* TODO: Sanity checks. */
	int size = 0;
	char **argv = get_argv(s, &size);
	char *in_name, *out_name, *err_name;
	int save_in, save_out, save_err;
	int status;

	in_name = get_word(s->in);
	out_name = get_word(s->out);
	err_name = get_word(s->err);

	save_in = dup(STDIN_FILENO);
	save_out = dup(STDOUT_FILENO);
	save_err = dup(STDERR_FILENO);

	/* TODO: If builtin command, execute the command. */
	if (!strcmp(argv[0], "cd")) {
		free_argv(argv, size);

		if (in_name) {
			redirect_input(s->io_flags, in_name);
			free(in_name);
		}

		if (out_name && err_name && !strcmp(out_name, err_name) && s->io_flags == 0) {
			creat(out_name, 0644);
			s->io_flags = 3;
		}

		if (out_name) {
			redirect_output(s->io_flags, out_name);
			free(out_name);
		}

		if (err_name) {
			redirect_error(s->io_flags, err_name);
			free(err_name);
		}

		status = shell_cd(s->params);

		dup2(save_in, 0);
		dup2(save_out, 1);
		dup2(save_err, 2);

		return status;
	}

	if (!strcmp(argv[0], "exit") || !strcmp(argv[0], "quit")) {
		free_argv(argv, size);

		if (in_name) {
			redirect_input(s->io_flags, in_name);
			free(in_name);
		}

		if (out_name && err_name && !strcmp(out_name, err_name) && s->io_flags == 0) {
			creat(out_name, 0644);
			s->io_flags = 3;
		}

		if (out_name) {
			redirect_output(s->io_flags, out_name);
			free(out_name);
		}

		if (err_name) {
			redirect_error(s->io_flags, err_name);
			free(err_name);
		}

		status = shell_exit();

		dup2(save_in, 0);
		dup2(save_out, 1);
		dup2(save_err, 2);

		return status;
	}

	close(save_in);
	close(save_out);
	close(save_err);

	if (strchr(argv[0], '=')) {
		char *variable_name;
		char *variable;
		char *value;

		variable_name = strtok_r(argv[0], "=", &(argv[0]));
		variable = strtok_r(argv[0], "=", &(argv[0]));

		if (variable[0] == '$') {
			variable[0] = '\0';
			variable++;
			value = getenv(variable);
		} else {
			value = variable;
		}
		status = setenv(variable_name, value, 1);
		free_argv(argv, size);
		return status;
	}

	/* TODO: If variable assignment, execute the assignment and return
	 * the exit status.
	 */

	/* TODO: If external command:
	 *   1. Fork new process
	 *     2c. Perform redirections in child
	 *     3c. Load executable in child
	 *   2. Wait for child
	 *   3. Return exit status
	 */
	int cpid = fork();

	if (!cpid) {
		if (in_name) {
			redirect_input(s->io_flags, in_name);
			free(in_name);
		}

		if (out_name && err_name && !strcmp(out_name, err_name) && s->io_flags == 0) {
			creat(out_name, 0644);
			s->io_flags = 3;
		}

		if (out_name) {
			redirect_output(s->io_flags, out_name);
			free(out_name);
		}

		if (err_name) {
			redirect_error(s->io_flags, err_name);
			free(err_name);
		}

		execvp(argv[0], (char *const *) argv);
		printf("Execution failed for '%s'\n", argv[0]);
		exit(EXIT_FAILURE);
	} else {
		waitpid(cpid, &status, 0);
	}

	free_argv(argv, size);
	return WEXITSTATUS(status); /* TODO: Replace with actual exit status. */
}

/**
 * Process two commands in parallel, by creating two children.
 */
static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	/* TODO: Execute cmd1 and cmd2 simultaneously. */
	int cpid1, cpid2;
	int status1, status2;

	cpid1 = fork();

	if (cpid1 == 0) {
		status1 = parse_command(cmd1, level, father);
		exit(status1);
	}


	cpid2 = fork();

	if (cpid2 == 0) {
		status2 = parse_command(cmd2, level, father);
		exit(status2);
	}

	waitpid(cpid1, &status1, 0);
	waitpid(cpid2, &status2, 0);

	return (WEXITSTATUS(status2) & WEXITSTATUS(status1)); /* TODO: Replace with actual exit status. */
}

/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2).
 */
static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	/* TODO: Redirect the output of cmd1 to the input of cmd2. */
	int cpid1, cpid2;
	int status1, status2;
	int pipes[2];
	int err;

	pipe(pipes);

	cpid1 = fork();

	if (cpid1 == 0) {
		err = close(pipes[0]);
		DIE(err < 0, "close");

		err = dup2(pipes[1], STDOUT_FILENO);
		DIE(err < 0, "dup2");

		close(pipes[1]);
		status1 = parse_command(cmd1, level, father);
		exit(status1);
	}

	close(pipes[1]);

	cpid2 = fork();

	if (cpid2 == 0) {
		err = dup2(pipes[0], STDIN_FILENO);
		DIE(err < 0, "dup2");

		close(pipes[0]);
		status2 = parse_command(cmd2, level, father);
		exit(status2);
	}

	waitpid(cpid2, &status2, 0);
	close(pipes[0]);

	return WEXITSTATUS(status2); /* TODO: Replace with actual exit status. */
}

/**
 * Parse and execute a command.
 */
int parse_command(command_t *c, int level, command_t *father)
{
	/* TODO: sanity checks */
	int ret;

	if (c->op == OP_NONE) {
		/* TODO: Execute a simple command. */

		return parse_simple(c->scmd, level, father); /* TODO: Replace with actual exit code of command. */
	}

	switch (c->op) {
	case OP_SEQUENTIAL:
		/* TODO: Execute the commands one after the other. */
		parse_command(c->cmd1, level + 1, c);
		return parse_command(c->cmd2, level + 1, c);

	case OP_PARALLEL:
		/* TODO: Execute the commands simultaneously. */
		run_in_parallel(c->cmd1, c->cmd2, level, father);
		break;

	case OP_CONDITIONAL_NZERO:
		/* TODO: Execute the second command only if the first one
		 * returns non zero.
		 */
		ret = parse_command(c->cmd1, level + 1, c);
		if (ret != 0)
			return parse_command(c->cmd2, level + 1, c);
		return ret;

	case OP_CONDITIONAL_ZERO:
		/* TODO: Execute the second command only if the first one
		 * returns zero.
		 */
		ret = parse_command(c->cmd1, level + 1, c);
		if (ret == 0)
			return parse_command(c->cmd2, level + 1, c);
		return ret;

	case OP_PIPE:
		/* TODO: Redirect the output of the first command to the
		 * input of the second.
		 */
		return run_on_pipe(c->cmd1, c->cmd2, level, father);

	default:
		return SHELL_EXIT;
	}

	return 0; /* TODO: Replace with actual exit code of command. */
}
