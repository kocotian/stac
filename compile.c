#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "arg.h"
#include "str.h"
#include "util.h"

/* Macros */
#define $(VARNAME) getVariableValue(#VARNAME)
#define DECLVAR(VARNAME, VALUE) \
	(vs[vss].name.len = strlen(vs[vss].name.data = #VARNAME), \
	 vs[vss].value.len = strlen(vs[vss].value.data = VALUE), \
	 ++vss)
#define BUFFER_SIZE 64 * 1024
#define VS_MAX 256

/* Types */
typedef struct {
	String name;
	String value;
} Variable;

/* Prototypes */
static void generateCode(int fd, String input, int c_mode);
static void generateC(int fd, String input);
static String getVariableValue(char *varname);
static void usage(void);

/* Globals */
char *argv0;

Variable vs[VS_MAX]; /* Variable stack */
size_t vss; /* Variable stack size */

/* Functions */
static void
generateCode(int fd, String input, int c_mode)
{
	if (c_mode) {
		write(fd, input.data, input.len);
	} else {
		int i = -1;
		write(fd, "write(fd, \"", 11);
		while (++i < input.len) {
			if (*input.data == '\n')
				dprintf(fd, "\"\n\"");
			dprintf(fd, "\\x%02x", *(input.data++));
		}
		dprintf(fd, "\", %d);", input.len);
	}
}

static void
generateC(int fd, String input)
{
	int c_mode, i, li;
	String ibuf;
	c_mode = 0;

	ibuf.data = input.data;
	ibuf.len = input.len;
	for (i = 0; i < input.len; ++i) {
		if (input.data[i] == '%') {
			ibuf.len = ++i - li - 1;
			generateCode(fd, ibuf, c_mode);
			ibuf.data = input.data + i;
			ibuf.len = input.len - i;
			c_mode = !c_mode;
			li = i;
		}
	}
	generateCode(fd, ibuf, c_mode);
}

static String
getVariableValue(char *v)
{
	size_t n;
	String s = { .data = v, .len = strlen(v) };
	n = vss;
	while (--n >= 0) {
		if (!Strcmp(vs[n].name, s))
			return vs[n].value;
	}
}

static void
usage(void)
{
	die("usage: %s [-v] [-o OUTPUT] INPUT [TEMPLATE]", argv0);
}

/* Main */
int
main(int argc, char *argv[])
{
	/* Variables: */
	/* File names and file descriptors */
	char *inputfn = NULL, *outputfn = NULL, *templatefn = NULL;
	int inputfd, outputfd, templatefd, viter;

	/* Read bytes (from nextline()/write()) */
	ssize_t rb;

	/* Input data */
	char input_buf[BUFFER_SIZE], idata[BUFFER_SIZE];

	String input = {
		.data = input_buf,
		.len = 0
	}, readinput, parseinput;

	/* Options parsing */
	ARGBEGIN {
	case 'o':
		outputfn = ARGF(); break;
	case 'v':
		die("generate-"VERSION); break;
	default:
		  usage(); break;
	} ARGEND

	/* Argument parsing */
	if (argc == 1)
		inputfn = argv[0];
	else if (argc == 2)
		templatefn = argv[1];
	else
		usage();

	if (outputfn == NULL) /* TODO */
		die("output not specified (needed in current stage)");

	/* Opening an input */
	if ((inputfd = open(inputfn, O_RDONLY)) < 0)
		die("open (input):");

	/* Zeroing first byte of input data */
	*(input.data) = '\0';

	/* Setting variable stack size to 0 */
	vss = 0;

	/* Initially, readinput points to beginning of all input */
	readinput.data = input.data;
	readinput.len = input.len;

	/* Declaring a few variables */
	DECLVAR(title, outputfn);
	DECLVAR(template, templatefn == NULL ? "none" : templatefn);

	/* Parsing ("preprocessing") input for variables */
	if ((rb = read(inputfd, idata, BUFFER_SIZE)) < 0)
		die("read (input):");

	readinput.data = idata;
	readinput.len  = rb;

	while (Strtok(readinput, &parseinput, '\n') > 0) {
		if (*(readinput.data) == '@' && *(readinput.data + 1) != '@') {
			++readinput.data;
			if (*(readinput.data) == '#'); /* comment */
			else { /* variable */
				String tok;
				if (Strtok(readinput, &tok, '=') <= 0)
					/* TODO: return syntax error */;
				vs[vss].name.data = (idata + (parseinput.data - idata) + 1);
				vs[vss].name.len = tok.len;
				vs[vss].value.data = (idata + (parseinput.data - idata) + 1) + (tok.len + 1);
				vs[vss].value.len = parseinput.len - (tok.len + 1) - 1;
				vs[vss].name = Strtrim(vs[vss].name);
				vs[vss].value = Strtrim(vs[vss].value);
				++vss;
			}
			--readinput.data;
		} else {
			strncat(input.data, readinput.data, MIN(BUFFER_SIZE - input.len, parseinput.len + 1));
			input.len += parseinput.len;
		}
		readinput.data += parseinput.len + 1;
		readinput.len -= parseinput.len;
	}

	/* Closing an input */
	close(inputfd);

	/* Here will be some templates replacing */

	/* Opening an output */
	if ((outputfd = open(outputfn, O_WRONLY | O_CREAT, 0644)) < 0)
		die("open (output):");

	/* Writing beginning of main() */
	write(outputfd, "#include <assemble.h>\nint main(void) {", 38);

	/* Declaring variables */
	for (viter = 0; viter < vss; ++viter)
		dprintf(outputfd, "DECLVAR(%.*s, \"%.*s\"); ",
				vs[viter].name.len,  vs[viter].name.data,
				vs[viter].value.len, vs[viter].value.data);

	/* And finally, generating C code to output */
	generateC(outputfd, input);

	/* After this, closing main() */
	write(outputfd, "\n}", 2);

	/* Closing an output */
	close(outputfd);

	return 0;
}