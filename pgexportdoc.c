/*-------------------------------------------------------------------------
 *
 * pgexportdoc.c
 *	  command line tool for export XML, JSON, BYTEA documents from PostgreSQL
 *
 * Author: Pavel Stehule, 2017 - Czech Republic, Prague
 *
 * IDENTIFICATION
 *   pgexportdoc.c 
 *
 *-------------------------------------------------------------------------
 */
#include "postgres_fe.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#include "libpq-fe.h"
#include "pg_getopt.h"
#include "pqexpbuffer.h"

#if PG_VERSION_NUM >= 140000

#include "common/string.h"

#endif


#if PG_VERSION_NUM >= 110000

#include "catalog/pg_type_d.h"

#else

#include "catalog/pg_type.h"

#endif

#define BUFSIZE			1024

enum trivalue
{
	TRI_DEFAULT,
	TRI_NO,
	TRI_YES
};

enum format
{
	FORMAT_XML,
	FORMAT_TEXT,
	FORMAT_BYTEA
};

struct _param
{
	char	   *pg_user;
	enum trivalue pg_prompt;
	char	   *pg_port;
	char	   *pg_host;
	const char *progname;
	int			verbose;
	enum format fmt;
	char	   *command;
	bool		use_stdout;
	char	   *filename;
	char	   *encoding;
};

static void usage(const char *progname);
/*
 * This exports stdout to target database
 */
static int
pgexportdoc(const char *database, const struct _param * param)
{
	PGconn	   *conn;
	bool		new_pass;
	static bool have_password = false;
	FILE	   *output;
	PGresult	*result = NULL;
	ExecStatusType status;

#if PG_VERSION_NUM >= 140000

	char	   *password = NULL;

#elif PG_VERSION_NUM >= 100000

	static char password[100];

#else

	char	   *password = NULL;

#endif

	/* Note: password can be carried over from a previous call */
	if (param->pg_prompt == TRI_YES && !have_password)
	{

#if PG_VERSION_NUM >= 140000

		password = simple_prompt("Password: ", false);

#elif PG_VERSION_NUM >= 100000

		simple_prompt("Password: ", password, sizeof(password), false);

#else

		password = simple_prompt("Password: ", 100, false);

#endif

		have_password = true;
	}

	/*
	 * Start the connection.  Loop until we have a password if requested by
	 * backend.
	 */
	do
	{
#define PARAMS_ARRAY_SIZE	   7

		const char *keywords[PARAMS_ARRAY_SIZE];
		const char *values[PARAMS_ARRAY_SIZE];

		keywords[0] = "host";
		values[0] = param->pg_host;
		keywords[1] = "port";
		values[1] = param->pg_port;
		keywords[2] = "user";
		values[2] = param->pg_user;
		keywords[3] = "password";
		values[3] = have_password ? password : NULL;
		keywords[4] = "dbname";
		values[4] = database;
		keywords[5] = "fallback_application_name";
		values[5] = param->progname;
		keywords[6] = NULL;
		values[6] = NULL;

		new_pass = false;

		conn = PQconnectdbParams(keywords, values, true);
		if (!conn)
		{
			fprintf(stderr, "Connection to database \"%s\" failed\n",
					database);
			return -1;
		}

		if (PQstatus(conn) == CONNECTION_BAD &&
			PQconnectionNeedsPassword(conn) &&
			!have_password &&
			param->pg_prompt != TRI_NO)
		{
			PQfinish(conn);

#if PG_VERSION_NUM >= 140000

			if (password)
				free(password);

			password = simple_prompt("Password: ", false);

#elif PG_VERSION_NUM >= 100000

			simple_prompt("Password: ", password, sizeof(password), false);

#else

			if (password)
				free(password);

			password = simple_prompt("Password: ", 100, false);

#endif

			have_password = true;
			new_pass = true;
		}
	} while (new_pass);

	/* check to see that the backend connection was successfully made */
	if (PQstatus(conn) == CONNECTION_BAD)
	{
		fprintf(stderr, "Connection to database \"%s\" failed:\n%s",
				database, PQerrorMessage(conn));
		PQfinish(conn);
		return -1;
	}

	if (param->verbose)
	{
		fprintf(stderr, "Connected to database \"%s\"\n", database);

		if (param->fmt == FORMAT_XML)
			fprintf(stderr, "export XML document\n");
		else if (param->fmt == FORMAT_TEXT)
			fprintf(stderr, "export TEXT document\n");
		else if (param->fmt == FORMAT_BYTEA)
			fprintf(stderr, "export BYTEA document\n");
	}

	if (param->encoding)
	{
		PQExpBufferData		setencoding;
		PGresult		   *setencresult;

		initPQExpBuffer(&setencoding);

		appendPQExpBuffer(&setencoding, "SET client_encoding TO %s",
						  param->encoding);

		if (param->verbose)
			fprintf(stderr, "execute command: %s\n", setencoding.data);

		setencresult = PQexec(conn, setencoding.data);

		status = PQresultStatus(setencresult);

		if (param->verbose)
		{
			fprintf(stderr, "Set encoding result status: %s\n", PQresStatus(status));
		}

		if (status != PGRES_COMMAND_OK)
		{
			fprintf(stderr, "%s: Unexpected result status: %s\n",
					param->progname, PQresStatus(status));
			fprintf(stderr, "%s: Error: %s\n",
					param->progname, PQresultErrorMessage(setencresult));
			PQfinish(conn);
			return -1;
		}

		PQclear(setencresult);
		termPQExpBuffer(&setencoding);
	}

	if (param->use_stdout)
	{
		output = stdout;
	}
	else
	{
		canonicalize_path(param->filename);

		output = fopen(param->filename,"wb");
		if (NULL == output)
		{
			fprintf(stderr, "%s: Unable to open '%s': %s\n",
				param->progname, param->filename, strerror(errno));
			PQfinish(conn);
			return -1;
		}
	}

	result = PQexecParams(conn,
							param->command,
							0, NULL, NULL, NULL, NULL,
							param->fmt == FORMAT_TEXT ? 0 : 1);

	status = PQresultStatus(result);

	if (param->verbose)
	{
		fprintf(stderr, "Result status: %s\n", PQresStatus(status));
	}

	if (status != PGRES_TUPLES_OK)
	{
		fprintf(stderr, "%s: Unexpected result status: %s\n",
				param->progname, PQresStatus(status));
		fprintf(stderr, "%s: Error: %s\n",
				param->progname, PQresultErrorMessage(result));
		PQfinish(conn);
		return -1;
	}

	/* check result */
	if (PQntuples(result) > 1 || PQnfields(result) > 1)
		fprintf(stderr, "pgexportdoc: warning: only first column of first row is displayed\n");

	if (PQntuples(result) > 0 && PQnfields(result) > 0)
	{
		char	   *value;
		int			length;
		Oid 		resultid;

		if (!PQgetisnull(result, 0, 0))
		{

			resultid = PQftype(result, 0);

			if ((param->fmt == FORMAT_XML && resultid != XMLOID) ||
				(param->fmt == FORMAT_BYTEA && resultid != BYTEAOID))
			{
				fprintf(stderr, "%s: Unexpected format of result value: %d\n",
						param->progname, resultid);
				PQfinish(conn);
				return -1;
			}
		}

		value = PQgetvalue(result, 0, 0);
		length = PQgetlength(result, 0, 0);
		fwrite(value, 1, length, output);

		if (ferror(output))
		{
			fprintf(stderr, "%s: Cannot write data '%s': %s\n",
					param->progname,
					param->filename ? param->filename : "stdout",
					strerror(errno));
			PQfinish(conn);
			return -1;
		}
	}

	if (!param->use_stdout)
		fclose(output);
	else
		fflush(output);

	PQclear(result);

	PQfinish(conn);

	return 0;
}

static void
usage(const char *progname)
{
	printf("%s exports XML, TEXT or BYTEA documents from PostgreSQL.\n\n", progname);
	printf("Usage:\n  %s [OPTION]... DBNAME\n\n", progname);
	printf("Options:\n");
	printf("  -V, --version  output version information, then exit\n");
	printf("  -?, --help     show this help, then exit\n");
	printf("  -E ENCODING    export text data in encoding ENCODING\n");
	printf("  -v             write a lot of progress messages\n");
	printf("  -c COMMAND     INSERT, UPDATE command with parameter\n");
	printf("  -f NAME        file NAME of exported document, default is stdout\n");
	printf("  -t TYPE        type specification [ XML | TEXT | BYTEA ], default is TEXT\n");
	printf("\nConnection options:\n");
	printf("  -h HOSTNAME    database server host or socket directory\n");
	printf("  -p PORT        database server port\n");
	printf("  -U USERNAME    user name to connect as\n");
	printf("  -w             never prompt for password\n");
	printf("  -W             force password prompt\n");
	printf("\n");
	printf("Report bugs to <pavel.stehule@gmail.com>.\n");
}

int
main(int argc, char **argv)
{
	int			rc = 0;
	struct _param param;
	int			c;
	int			port;
	const char *progname;

	progname = get_progname(argv[0]);

	/* Set default parameter values */
	param.verbose = 0;
	param.pg_user = NULL;
	param.pg_prompt = TRI_DEFAULT;
	param.fmt = FORMAT_TEXT;
	param.pg_host = NULL;
	param.pg_port = NULL;
	param.progname = progname;
	param.use_stdout = true;
	param.filename = NULL;
	param.command = NULL;
	param.encoding = NULL;

	/* Process command-line arguments */
	if (argc > 1)
	{
		if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
		{
			usage(progname);
			exit(0);
		}
		if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
		{
			puts("pgexportdoc (PostgreSQL) " PG_VERSION);
			exit(0);
		}
	}

	while (1)
	{
		c = getopt(argc, argv, "E:h:f:U:p:c:t:vwW");
		if (c == -1)
			break;

		switch (c)
		{
			case '?':
				fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
				exit(1);
			case ':':
				exit(1);
			case 'v':
				param.verbose = 1;
				break;
			case 'c':
				param.command = pg_strdup(optarg);
				break;
			case 'f':
				if (strcmp(optarg, "-") != 0)
				{
					param.filename = pg_strdup(optarg);
					param.use_stdout = false;
				}
				break;
			case 't':
				if (strcmp(optarg, "XML") == 0)
					param.fmt = FORMAT_XML;
				else if (strcmp(optarg, "TEXT") == 0)
					param.fmt = FORMAT_TEXT;
				else if (strcmp(optarg, "BYTEA") == 0)
					param.fmt = FORMAT_BYTEA;
				else
				{
					fprintf(stderr,
							"%s: only XML, TEXT or BYTEA types are supported\n",
							progname);
					exit(1);
				}
				break;
			case 'E':
				param.encoding = pg_strdup(optarg);
				break;
			case 'U':
				param.pg_user = pg_strdup(optarg);
				break;
			case 'w':
				param.pg_prompt = TRI_NO;
				break;
			case 'W':
				param.pg_prompt = TRI_YES;
				break;
			case 'p':
				port = strtol(optarg, NULL, 10);
				if ((port < 1) || (port > 65535))
				{
					fprintf(stderr, "%s: invalid port number: %s\n", progname, optarg);
					exit(1);
				}
				param.pg_port = pg_strdup(optarg);
				break;
			case 'h':
				param.pg_host = pg_strdup(optarg);
				break;
		}
	}

	if (param.command == NULL)
	{
		fprintf(stderr, "pgexportdoc: missing required argument: -c COMMAND\n");
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
		exit(1);
	}

	/* No database given? Show usage */
	if (optind + 1 !=  argc)
	{
		fprintf(stderr, "pgexportdoc: missing required argument: database name\n");
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
		exit(1);
	}

	if (param.encoding != NULL && param.fmt == FORMAT_BYTEA)
	{
		fprintf(stderr, "pgexportdoc: warning: encoding is used only for type TEXT and XML\n");
	}

	rc = pgexportdoc(argv[argc - 1], &param);
	return rc;
}
