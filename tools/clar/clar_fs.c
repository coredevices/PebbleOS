/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef _WIN32

#define FOF_FLAGS (FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR)

static char *
fileops_path(const char *_path)
{
	char *path = NULL;
	size_t length, i;

	if (_path == NULL)
		return NULL;

	length = strlen(_path);
	path = malloc(length + 2);

	if (path == NULL)
		return NULL;

	memcpy(path, _path, length);
	path[length] = 0;
	path[length + 1] = 0;

	for (i = 0; i < length; ++i) {
		if (path[i] == '/')
			path[i] = '\\';
	}

	return path;
}

static void
fileops(int mode, const char *_source, const char *_dest)
{
	SHFILEOPSTRUCT fops;

	char *source = fileops_path(_source);
	char *dest = fileops_path(_dest);

	ZeroMemory(&fops, sizeof(SHFILEOPSTRUCT));

	fops.wFunc = mode;
	fops.pFrom = source;
	fops.pTo = dest;
	fops.fFlags = FOF_FLAGS;

	cl_assert_(
		SHFileOperation(&fops) == 0,
		"Windows SHFileOperation failed"
	);

	free(source);
	free(dest);
}

static void
fs_rm(const char *_source)
{
	fileops(FO_DELETE, _source, NULL);
}

static void
fs_copy(const char *_source, const char *_dest)
{
	fileops(FO_COPY, _source, _dest);
}

void
cl_fs_cleanup(void)
{
	fs_rm(fixture_path(_clar_path, "*"));
}

#else

#ifdef EMSCRIPTEN
#include <emscripten/emscripten.h>
// fork() is not supported by Emscripten, so try to use Node's child_process.execSync
static int
shell_out(char * const argv[])
{
  return EM_ASM_INT({
    var child_process = require('child_process');
    try {
      var args = [];
      for (var i = 0;; i++) {
        var argStrPtr = getValue($0 + i*4, '*');
        if (argStrPtr === 0) {
          break;
        }
        var arg = Pointer_stringify(argStrPtr);
        args.push(arg);
      }
      // FIXME: need to escape args?
      var out = child_process.execSync(args.join(' '));
      return 0;
    } catch (e) {
      console.error(e);
      return -1;
    }
  }, argv);
}
#else  // #ifdef EMSCRIPTEN
static int
shell_out(char * const argv[])
{
	int status;
	pid_t pid;

	pid = fork();

	if (pid < 0) {
		fprintf(stderr,
			"System error: `fork()` call failed.\n");
		exit(-1);
	}

	if (pid == 0) {
		execv(argv[0], argv);
	}

	waitpid(pid, &status, 0);
	return WEXITSTATUS(status);
}
#endif

#include <string.h>

static void
fs_copy(const char *_source, const char *dest)
{
	char *argv[5];
	char *source;
	size_t source_len;

	source = strdup(_source);
	source_len = strlen(source);

	if (source[source_len - 1] == '/')
		source[source_len - 1] = 0;

	argv[0] = "/bin/cp";
	argv[1] = "-R";
	argv[2] = source;
	argv[3] = (char *)dest;
	argv[4] = NULL;

	cl_must_pass_(
		shell_out(argv),
		"Failed to copy test fixtures to sandbox"
	);

	free(source);
}

static void
fs_rm(const char *source)
{
	char *argv[4];

	argv[0] = "/bin/rm";
	argv[1] = "-Rf";
	argv[2] = (char *)source;
	argv[3] = NULL;

	cl_must_pass_(
		shell_out(argv),
		"Failed to cleanup the sandbox"
	);
}

void
cl_fs_cleanup(void)
{
	clar_unsandbox();
	clar_sandbox();
}
#endif
