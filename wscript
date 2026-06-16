import os
import re
import subprocess
import sys
import datetime
import time

import waflib
from waflib import Logs
from waflib.Build import BuildContext


def _normalize_kconfig_override_args(argv):
    normalized = []
    for arg in argv:
        if arg.startswith('-DCONFIG_') and '=' in arg:
            normalized.append('--kconfig-override={}'.format(arg[2:]))
        else:
            normalized.append(arg)
    return normalized


sys.argv = _normalize_kconfig_override_args(sys.argv)

waf_dir = sys.path[0]
sys.path.append(os.path.join(waf_dir, 'tools'))
sys.path.append(os.path.join(waf_dir, 'tools/log_hashing'))
sys.path.append(os.path.join(waf_dir, 'sdk/tools/'))
sys.path.append(os.path.join(waf_dir, 'waftools'))

import waftools.gitinfo
import waftools.boards
import waftools.ldscript
import waftools.pebble_sdk_gcc as pebble_sdk_gcc
import tools.runners as pebble_runners
from waftools.pebble_sdk_locator import activate_sdk

from pebble_sdk_version import set_env_sdk_version

# Prefer an installed PebbleOS SDK's binaries (toolchain, QEMU, sftool) when
# present. Done at import time so it applies to every waf invocation.
activate_sdk(waflib.Context.run_dir or os.getcwd())

LOGHASH_OUT_PATH = 'src/fw/loghash_dict.json'

def _available_boards():
    return waftools.boards.available_boards(waflib.Context.run_dir or os.getcwd())


def truncate(msg):
    if msg is None:
        return msg

    # Don't truncate exceptions thrown by waf itself
    if "Traceback " in msg:
        return msg

    truncate_length = 600
    if len(msg) > truncate_length:
        msg = msg[:truncate_length-4] + '...\n' + waflib.Logs.colors.NORMAL
    return msg


class _OptParserAdapter(object):
    """Adapts a waf (optparse) option container to the argparse-style
    add_argument() interface the runners' do_add_parser() expects."""

    def __init__(self, opt):
        self._opt = opt

    def add_argument(self, *flags, **kwargs):
        self._opt.add_option(*flags, **kwargs)


def options(opt):
    opt.load('pebble_arm_gcc', tooldir='waftools')
    opt.load('show_configure', tooldir='waftools')
    opt.load('kconfig', tooldir='waftools')
    opt.recurse('src/fw')

    gr = opt.add_option_group('test options')
    gr.add_option('-D', '--debug_test', action='store_true',
        help='Execute tests within GDB. Use alongside -M.')
    gr.add_option('-M', '--match', dest='regex', default=None, action='store',
        help='Run regex match tests. Example: ./waf test -M "test.*resource.*"')
    gr.add_option('-L', '--list_tests', dest='list_tests', action='store_true',
        help='List all test names. Usually used in conjunction with -M. Example: '
             './waf test -M test_animation -L')
    gr.add_option('-T', '--test_name', dest='test_name', default=None, action='store',
        help='Run only the given test name. Usually used in conjunction with -M. Example: '
             './waf test -M test_animation -T unschedule')
    gr.add_option('-C', '--coverage', dest='coverage', action='store_true', help='Generate gcov test coverage data and use lcov to generate HTML report')
    gr.add_option('--show_output', action='store_true', help='show test output')
    gr.add_option('--no_run', action='store_true', help='Do not run the tests, just build them')
    gr.add_option('--no_images', action='store_true', help='skip generation of test images, '
                  'which are only required for some tests and can slow down build times')
    boards = _available_boards()
    opt.add_option('--board', action='store',
                   choices=boards,
                   help='Which board we are targeting '
                        '({})'.format(', '.join(boards)))
    opt.add_option('--runner', default=None,
                   help="Override the board's default runner for flash/run/debug")
    opt.add_option('--resources', action='store_true',
                   help='Also flash system resources alongside the firmware')
    # Runner-specific arguments (e.g. --tty for sftool) are contributed by the
    # runners themselves, mirroring Zephyr's west do_add_parser().
    pebble_runners.register_args(_OptParserAdapter(opt))
    opt.add_option('--compile_commands', action='store_true', help='Create a clang compile_commands.json')
    opt.add_option('--onlysdk', action='store_true', help="only build the sdk")
    opt.add_option('--no-link', action='store_true',
                   help='Do not link the final firmware binary. This is used for static analysis')
    opt.add_option('--variant', action='store', default='normal',
                   choices=['normal', 'prf'],
                   help='Build variant: normal (default) or prf (recovery firmware)')

def handle_configure_options(conf):
    if conf.options.no_link:
        conf.env.NO_LINK = True
        print("Not linking firmware")

def configure(conf):
    if not conf.options.board:
        conf.fatal('No board selected! '
                   'You must pass a --board argument when configuring.')

    try:
        board = waftools.boards.parse_board(conf.srcnode.abspath(), conf.options.board)
    except ValueError as e:
        conf.fatal(str(e))

    # Has to be 'waftools.gettext' as unadorned 'gettext' will find the gettext
    # module in the standard library.
    conf.load('waftools.gettext')

    conf.load('kconfig', tooldir='waftools')

    # JS engine selection is driven entirely by CONFIG_MODDABLE_XS. Override
    # per-board with `-DCONFIG_MODDABLE_XS=y/n` at configure time.
    if conf.env.CONFIG_MODDABLE_XS:
        conf.env.JS_ENGINE = 'moddable'
    else:
        conf.env.JS_ENGINE = 'none'

    if not board.runners and not conf.env.CONFIG_QEMU:
        conf.fatal('Board {} does not define any supported runners'.format(
                   board.target))

    for runner in board.runners:
        if runner not in pebble_runners.names():
            conf.fatal('Board {} references unknown runner {}'.format(
                       board.target, runner))

    conf.env.SUPPORTED_RUNNERS = board.runners
    conf.env.RUNNER = board.runners[0] if board.runners else None

    conf.env.FLASH_ITCM = False

    # Set platform used for building the SDK
    if conf.env.CONFIG_PLATFORM_EMERY:
        conf.env.PLATFORM_NAME = 'emery'
        conf.env.MIN_SDK_VERSION = 3
    elif conf.env.CONFIG_PLATFORM_FLINT:
        conf.env.PLATFORM_NAME = 'flint'
        conf.env.MIN_SDK_VERSION = 2
    elif conf.env.CONFIG_PLATFORM_GABBRO:
        conf.env.PLATFORM_NAME = 'gabbro'
        conf.env.MIN_SDK_VERSION = 3
    else:
        conf.fatal('No platform specified for {}!'.format(board.target))

    # Save this for later
    conf.env.BOARD = board.target
    conf.env.BOARD_NAME = board.name
    conf.env.BOARD_REVISION = board.revision
    conf.env.BOARD_NORMALIZED = board.normalized

    conf.env.VARIANT = conf.options.variant
    if conf.env.VARIANT == 'prf':
        conf.env.JS_ENGINE = 'none'

    # PRF variant forces JS_ENGINE='none' above. If the board's defconfig had
    # CONFIG_MODDABLE_XS=y, autoconf.h was already written with the macro
    # defined — undefine it on the command line so source-level guards match
    # what we actually link.
    if conf.env.JS_ENGINE == 'none' and conf.env.CONFIG_MODDABLE_XS:
        conf.env.append_value('CFLAGS', ['-UCONFIG_MODDABLE_XS'])
        conf.env.CONFIG_MODDABLE_XS = None

    conf.find_program('node nodejs', var='NODE',
                      errmsg="Unable to locate the Node command. "
                             "Please check your Node installation and try again.")

    conf.load('protoc')
    conf.recurse('src/fw')

    # Save a baseline environment that we'll use for unit tests
    # Detach so operations against conf.env don't affect unit_test_env
    unit_test_env = conf.env.derive()
    unit_test_env.detach()

    # Save a baseline environment that we'll use for ARM environments
    base_env = conf.env

    handle_configure_options(conf)

    Logs.pprint('CYAN', 'Configuring arm_firmware environment')
    conf.setenv('', base_env)
    conf.load('pebble_arm_gcc', tooldir='waftools')

    Logs.pprint('CYAN', 'Configuring unit test environment')
    conf.setenv('local', unit_test_env)

    # Strip CONFIG_* DEFINES mirrored from the configure-time board: each test
    # selects its own simulated platform (asterix / obelix / gabbro) and injects
    # the matching BOARD_FAMILY/PLATFORM/SCREEN_COLOR_DEPTH_BITS itself, so the
    # configure board's symbols would just collide with the per-test ones.
    conf.env.DEFINES = [d for d in conf.env.DEFINES
                        if not d.split('=', 1)[0].startswith('CONFIG_')]

    # if sys.platform.startswith('linux'):
        # libclang_path = subprocess.check_output(['llvm-config', '--libdir']).strip()
        # conf.env.append_value('INCLUDES', [os.path.join(libclang_path, 'clang/3.2/include/'),])

    # The waf clang tool likes to use llvm-ar as it's ar tool, but that doesn't work on our build
    # servers. Fall back to boring old ar. This will populate the 'AR' env variable so future
    # searches for what value to put into env['AR'] will find this one.
    conf.find_program('ar')

    conf.load('clang')
    conf.load('pebble_test', tooldir='waftools')

    conf.env.CLAR_DIR = conf.path.make_node('tools/clar/').abspath()
    conf.env.CFLAGS = [ '-std=c11',
                        '-Wall',
                        '-Werror',
                        '-Wno-error=unused-variable',
                        '-Wno-error=unused-function',
                        '-Wno-error=missing-braces',
                        '-Wno-error=unused-const-variable',
                        '-Wno-error=address-of-packed-member',
                        '-Wno-enum-conversion',

                        '-g3',
                        '-gdwarf-4',
                        '-O0',
                        '-fdata-sections',
                        '-ffunction-sections',
                        '-fno-common',
                        '-ffp-contract=off',
                        '-fexcess-precision=standard' ]

    # Reset LINKFLAGS so firmware-specific flags (e.g. --undefined=HAL_GetTick)
    # don't leak into the host test environment.
    conf.env.LINKFLAGS = []

    # Apple's ARM64 linker uses chained fixups which require pointer-aligned
    # relocations. Packed structs with pointer members fail to link because the
    # packed layout can place pointers at non-aligned offsets. Disable chained
    # fixups to use classic relocations instead.
    if sys.platform == 'darwin':
        conf.env.append_value('LINKFLAGS', '-Wl,-no_fixup_chains')

    conf.env.append_value('DEFINES', 'CLAR_FIXTURE_PATH="' +
                                     conf.path.make_node('tests/fixtures/').abspath() + '"')

    conf.env.append_value('DEFINES', 'CONFIG_LOG=1')

    if conf.options.compile_commands:
        conf.load('clang_compilation_database', tooldir='waftools')

        if not os.path.lexists('compile_commands.json'):
            filename = 'compile_commands.json'
            source = conf.path.get_bld().make_node(filename)
            os.symlink(source.path_from(conf.path), filename)

    Logs.pprint('CYAN', 'Configuring stored apps environment')
    conf.setenv('stored_apps', base_env)
    process_info = conf.path.find_node('src/fw/process_management/pebble_process_info.h')
    set_env_sdk_version(conf, process_info)
    pebble_sdk_gcc.configure(conf)

    # Confirm that requirements-*.txt and requirements-osx-brew.txt have been satisfied.
    import tool_check
    tool_check.tool_check()


def stop_build_timer(ctx):
    t = datetime.datetime.utcnow() - ctx.pbl_build_start_time
    node = ctx.path.get_bld().make_node('build_time')
    with open(node.abspath(), 'w') as fout:
        fout.write(str(int(round(t.total_seconds()))))


def build(bld):
    bld.DYNAMIC_RESOURCES = []
    bld.LOGHASH_DICTS = []

    # Start this timer here to include the time to generate tasks.
    bld.pbl_build_start_time = datetime.datetime.utcnow()
    bld.add_post_fun(stop_build_timer)

    # FIXME: remove include/pbl once all modules use prefix
    bld(export_includes=['include', 'include/pbl'], name='pbl_includes')

    if bld.variant == 'test':
        bld.set_env(bld.all_envs['local'])

    bld.load('file_name_c_define', tooldir='waftools')

    bld.recurse('third_party/nanopb')
    bld.recurse('src/idl')

    if bld.cmd == 'install':
        raise Exception("install isn't a supported command. Did you mean flash?")

    if bld.variant == 'pdc2png':
        bld.recurse('tools')
        return

    if bld.variant == 'tools':
        bld.recurse('tools')
        return

    if bld.variant == '':
        # Dependency for SDK
        bld.recurse('third_party/moddable')

    if bld.variant == '' and bld.env.VARIANT != 'prf':
        # sdk generation
        bld.recurse('sdk')

    if bld.options.onlysdk:
        # stop here, sdk generation is done
        return

    # Do not enable stationary mode in PRF or release firmware
    if (bld.env.VARIANT != 'prf' and not bld.env.CONFIG_QEMU and bld.env.NORMAL_SHELL != 'sdk'):
        bld.env.append_value('DEFINES', 'STATIONARY_MODE')

    if bld.variant == 'test':
        bld.recurse('third_party/nanopb')
        bld.recurse('src/libbtutil')
        bld.recurse('src/libos')
        bld.recurse('src/libutil')
        bld.recurse('tests')
        bld.recurse('tools')
        return

    if bld.variant == '' and bld.env.VARIANT != 'prf':
        bld.recurse('stored_apps')

    bld.recurse('third_party')
    bld.recurse('src/libbtutil')
    bld.recurse('src/bluetooth-fw')
    bld.recurse('src/libc')
    bld.recurse('src/libos')
    bld.recurse('src/libutil')
    bld.recurse('src/fw')

    # Generate resources. Leave this until the end so we collect all the env['DYNAMIC_RESOURCES']
    # values that the other build steps added.
    bld.recurse('resources')

    # if we're not linking the firmware don't run these
    if not bld.env.NO_LINK:
        bld.add_post_fun(size_fw)
        bld.add_post_fun(size_resources)
        if bld.env.CONFIG_LOG_HASHED:
            bld.add_post_fun(merge_loghash_dicts)


def merge_loghash_dicts(bld):
    loghash_dict = bld.path.get_bld().make_node(LOGHASH_OUT_PATH)

    import log_hashing.newlogging
    log_hashing.newlogging.merge_loghash_dict_json_files(loghash_dict, bld.LOGHASH_DICTS)


class SizeFirmware(BuildContext):
    cmd = 'size_fw'
    fun = 'size_fw'

def size_fw(ctx):
    """prints size information of the firmware"""

    fw_elf = ctx.get_tintin_fw_node().change_ext('.elf')
    if fw_elf is None:
        ctx.fatal('No fw ELF found for size')

    fw_bin = ctx.get_tintin_fw_node()
    if fw_bin is None:
        ctx.fatal('No fw BIN found for size')

    import binutils
    text, data, bss = binutils.size(fw_elf.abspath())
    total = text + data
    output = ('{:>7}    {:>7}    {:>7}    {:>7}    {:>7} filename\n'
              '{:7}    {:7}    {:7}    {:7}    {:7x} tintin_fw.elf'.
              format('text', 'data', 'bss', 'dec', 'hex', text, data, bss, total, total))
    Logs.pprint('YELLOW', '\n' + output)

    try:
        space_left = _check_firmware_image_size(ctx, fw_bin.path_from(ctx.path))
    except FirmwareTooLargeException as e:
        ctx.fatal(str(e))
    else:
        Logs.pprint('CYAN', 'FW: ' + space_left)


class SizeResources(BuildContext):
    cmd = 'size_resources'
    fun = 'size_resources'


def size_resources(ctx):
    """prints size information of resources"""

    if ctx.env.VARIANT == 'prf':
        return

    pbpack_path = ctx.path.get_bld().find_node('system_resources.pbpack')
    if pbpack_path is None:
        ctx.fatal('No resource pbpack found')

    if ctx.env.CONFIG_SOC_NRF52:
        max_size = 1024 * 1024
    elif ctx.env.CONFIG_SOC_SF32LB52:
        max_size = 2048 * 1024
    elif ctx.env.CONFIG_QEMU:
        max_size = 2048 * 1024
    else:
        max_size = 256 * 1024

    pbpack_actual_size = os.path.getsize(pbpack_path.path_from(ctx.path))
    bytes_free = max_size - pbpack_actual_size

    from waflib import Logs
    Logs.pprint('CYAN', 'Resources: %d/%d (%d free)\n' % (pbpack_actual_size, max_size, bytes_free))

    if pbpack_actual_size > max_size:
        ctx.fatal('Resources are too large for target board %d > %d'
                  % (pbpack_actual_size, max_size))


def size(ctx):
    from waflib import Options
    Options.commands = ['size_fw', 'size_resources'] + Options.commands


class test(BuildContext):
    """builds and runs the tests"""
    cmd = 'test'
    variant = 'test'



def docs(ctx):
    """builds the documentation out to build/doxygen"""
    ctx.exec_command('doxygen Doxyfile', stdout=None, stderr=None)


class DocsSdk(BuildContext):
    """builds the sdk documentation out to build/sdk/<platformname>/doxygen_sdk"""
    cmd = 'docs_sdk'
    fun = 'docs_sdk'


def docs_sdk(ctx):
    pebble_sdk = ctx.path.get_bld().make_node('sdk')
    supported_platforms = pebble_sdk.listdir()

    for platform in supported_platforms:
        doxyfile = pebble_sdk.find_node(platform).find_node('Doxyfile-SDK.auto')
        if doxyfile:
            ctx.exec_command('doxygen {}'.format(doxyfile.path_from(ctx.path)),
                             stdout=None, stderr=None)


def docs_all(ctx):
    """builds the documentation with all dependency graphs out to build/doxygen"""
    ctx.exec_command('doxygen Doxyfile-all-graphs', stdout=None, stderr=None)

# Bundle commands
#################################################


def _get_version_info(ctx):
    # FIXME: it's probably a better idea to lift board + version info from the .bin file... this can get out of sync!
    git_revision = waftools.gitinfo.get_git_revision(ctx)
    if git_revision['TAG'] != '?':
        version_string = git_revision['TAG']
        version_ts = int(git_revision['TIMESTAMP'])
        version_commit = git_revision['COMMIT']
    else:
        version_string = 'dev'
        version_ts = 0
        version_commit = ''
    return version_string, version_ts, version_commit


def _make_bundle(ctx, fw_bin_path, fw_type='normal', board=None, resource_path=None, write=True):
    import mkbundle

    if board is None:
        board = ctx.env.BOARD_NORMALIZED

    b = mkbundle.PebbleBundle()

    version_string, version_ts, version_commit = _get_version_info(ctx)
    slot = ctx.env.SLOT if fw_type == 'normal' and ctx.env.SLOT != -1 else None
    out_file = ctx.get_pbz_node(fw_type, ctx.env.BOARD_NORMALIZED, version_string, slot).path_from(ctx.path)

    try:
        _check_firmware_image_size(ctx, fw_bin_path)
        b.add_firmware(fw_bin_path, fw_type, version_ts, version_commit, board, version_string, slot)
    except FirmwareTooLargeException as e:
        ctx.fatal(str(e))
    except mkbundle.MissingFileException as e:
        ctx.fatal('Error: Missing file ' + e.filename + ', have you run ./waf build yet?')

    if resource_path is not None:
        b.add_resources(resource_path, version_ts)
    if not ctx.env.CONFIG_RELEASE and ctx.env.CONFIG_LOG_HASHED:
        loghash_dict = ctx.path.get_bld().make_node(LOGHASH_OUT_PATH).abspath()
        b.add_loghash(loghash_dict)

    # Add a LICENSE.txt file
    b.add_license('LICENSE')

    if fw_type == 'normal':
        layouts_node = ctx.path.get_bld().find_node('resources/layouts.json.auto')
        if layouts_node is not None:
            b.add_layouts(layouts_node.path_from(ctx.path))

    if write:
        b.write(out_file)
        waflib.Logs.pprint('CYAN', 'Writing bundle to: %s' % out_file)

    return b


class BundleCommand(BuildContext):
    cmd = 'bundle'
    fun = 'bundle'


def bundle(ctx):
    """bundles a firmware"""

    if ctx.env.VARIANT == 'prf':
        _make_bundle(ctx, ctx.get_tintin_fw_node().path_from(ctx.path), fw_type='recovery')
    else:
        _make_bundle(ctx, ctx.get_tintin_fw_node().path_from(ctx.path),
                     resource_path=ctx.get_pbpack_node().path_from(ctx.path))


# QEMU flash image commands
#################################################

class QemuImageMicroCommand(BuildContext):
    cmd = 'qemu_image_micro'
    fun = 'qemu_image_micro'


class QemuImageSpiCommand(BuildContext):
    cmd = 'qemu_image_spi'
    fun = 'qemu_image_spi'


def qemu_image_micro(ctx):
    """creates the micro-flash image for qemu"""
    from intelhex import IntelHex

    fw_hex = ctx.get_tintin_fw_node().change_ext('.hex')
    micro_flash_node = ctx.path.get_bld().make_node('qemu_micro_flash.bin')
    micro_flash_path = micro_flash_node.path_from(ctx.path)
    Logs.pprint('CYAN', 'Writing micro flash image to {}'.format(micro_flash_path))

    img = IntelHex(fw_hex.path_from(ctx.path))
    img.padding = 0xff
    flash_end = ((img.maxaddr() + 511) // 512) * 512
    img.tobinfile(micro_flash_path, start=0x00000000, end=flash_end - 1)


def qemu_image_spi(ctx):
    """creates a SPI flash image for qemu"""
    if ctx.env.CONFIG_QEMU:
        # QEMU generic boards: resources at offset 0x620000 in 32MB flash
        resources_begin = 0x620000
        image_size = 0x2000000
    else:
        resources_begin = 0x280000
        image_size = 0x400000

    spi_flash_node = ctx.path.get_bld().make_node('qemu_spi_flash.bin')
    spi_flash_path = spi_flash_node.path_from(ctx.path)
    Logs.pprint('CYAN', 'Writing SPI flash image to {}'.format(spi_flash_path))
    with open(spi_flash_path, 'wb') as qemu_spi_img_file:
        # Pad the first section before system resources with FF's
        qemu_spi_img_file.write(bytes([0xff]) * resources_begin)

        # Write system resources:
        pbpack = ctx.get_pbpack_node()
        res_img = open(pbpack.path_from(ctx.path), 'rb').read()
        qemu_spi_img_file.write(res_img)

        # Pad with 0xFF up to image size
        tail_padding_size = image_size - resources_begin - len(res_img)
        qemu_spi_img_file.write(bytes([0xff]) * tail_padding_size)


# Flash commands
#################################################

class FirmwareTooLargeException(Exception):
    pass


def _check_firmware_image_size(ctx, path):
    BYTES_PER_K = 1024
    firmware_size = os.path.getsize(path)
    # Determine flash and bootloader size so we can calculate the max firmware size
    if ctx.env.CONFIG_SOC_NRF52:
        if ctx.env.VARIANT == 'prf' and not ctx.env.CONFIG_MFG:
            max_firmware_size = 512 * BYTES_PER_K
        else:
            # 1024k of flash and 32k bootloader
            max_firmware_size = (1024 - 32) * BYTES_PER_K
    elif ctx.env.CONFIG_SOC_SF32LB52:
        if ctx.env.VARIANT == 'prf' and not ctx.env.CONFIG_MFG:
            max_firmware_size = 576 * BYTES_PER_K
        else:
            # 3072k of flash
            max_firmware_size = 3072 * BYTES_PER_K
    elif ctx.env.CONFIG_QEMU:
        max_firmware_size = 4096 * BYTES_PER_K
    else:
        ctx.fatal('Cannot check firmware size against unknown micro family')

    if firmware_size > max_firmware_size:
        raise FirmwareTooLargeException('Firmware is too large! Size is 0x%x should be less than 0x%x' \
                                        % (firmware_size, max_firmware_size))

    return ('%d / %d bytes used (%d free)' %
            (firmware_size, max_firmware_size, (max_firmware_size - firmware_size)))


def _create_runner(ctx, want_resources=False):
    selected = ctx.options.runner or ctx.env.RUNNER
    supported = ctx.env.SUPPORTED_RUNNERS or ([ctx.env.RUNNER] if ctx.env.RUNNER else [])

    if not selected:
        ctx.fatal('No runner available for board {}'.format(ctx.env.BOARD))
    if selected not in supported:
        ctx.fatal('Board {} does not support runner {}. Supported runners: {}'.format(
                  ctx.env.BOARD, selected, ', '.join(supported) or 'none'))

    resources_file = None
    if want_resources and ctx.options.resources and ctx.env.VARIANT != 'prf':
        resources_file = ctx.get_pbpack_node().path_from(ctx.path)

    fw = ctx.get_tintin_fw_node()
    cfg = pebble_runners.RunnerConfig(
        board_dir=os.path.join('boards', ctx.env.BOARD_NAME),
        soc=ctx.env.CONFIG_SOC,
        hex_file=fw.change_ext('.hex').path_from(ctx.path),
        elf_file=fw.change_ext('.elf').path_from(ctx.path),
        resources_file=resources_file,
    )

    try:
        return pebble_runners.create(selected, cfg, ctx.options)
    except pebble_runners.RunnerError as e:
        ctx.fatal(str(e))


class FlashCommand(BuildContext):
    """flashes the firmware to a connected device"""
    cmd = 'flash'
    fun = 'flash'


def flash(ctx):
    fw_bin = ctx.get_tintin_fw_node()
    try:
        space_left = _check_firmware_image_size(ctx, fw_bin.path_from(ctx.path))
    except FirmwareTooLargeException as e:
        ctx.fatal(str(e))
    Logs.pprint('CYAN', 'FW: ' + space_left)

    runner = _create_runner(ctx, want_resources=True)
    try:
        runner.run('flash')
    except pebble_runners.RunnerError as e:
        ctx.fatal(str(e))


class RunCommand(BuildContext):
    """resets and runs the firmware on a connected device"""
    cmd = 'run'
    fun = 'run'


def run(ctx):
    try:
        _create_runner(ctx).run('run')
    except pebble_runners.RunnerError as e:
        ctx.fatal(str(e))


class DebugCommand(BuildContext):
    """attaches gdb to the target"""
    cmd = 'debug'
    fun = 'debug'


def debug(ctx):
    try:
        _create_runner(ctx).run('debug')
    except pebble_runners.RunnerError as e:
        ctx.fatal(str(e))


# Tool build commands
#################################################


class build_pdc2png(BuildContext):
    """executes the pdc2png build"""
    cmd = 'build_pdc2png'
    variant = 'pdc2png'


class build_tools(BuildContext):
    """build all tools in tools/ dir"""
    cmd = 'build_tools'
    variant = 'tools'

# vim:filetype=python
