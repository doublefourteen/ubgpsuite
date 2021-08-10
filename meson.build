project('µbgpsuite', 'c', version : '0.1',
                          meson_version : '>=0.52.0',
                          license : ['LGPL-3.0-or-later', 'GPL-3.0-or-later'],
                          default_options : [ 'warning_level=3',
                                              'b_ndebug=if-release',
                                              'b_lto=true'
                                            ])

cc = meson.get_compiler('c')
add_project_arguments('-D_GNU_SOURCE', language: 'c')
if cc.has_argument('-Wno-missing-field-initializers')
    # We know missing fields are zeroed out...
    add_project_arguments('-Wno-missing-field-initializers', language: 'c')
endif
if cc.has_link_argument('-rdynamic')
    # Useful for dbg.h
    add_project_link_arguments('-rdynamic', language: 'c')
endif
if get_option('native')
    # Local CPU optimized build, warrants a message, this option is pretty extreme
    native_flags = [ '-march=native', '-mtune=native' ]

    message('Enabling native local CPU build flags')
    add_project_arguments(cc.get_supported_arguments(native_flags), language: 'c')
endif

lonetix_args = []

zlib_dep = dependency('zlib')
bz2_dep  = cc.find_library('bz2', required : true)
lzma_dep = dependency('liblzma', version: '>=5.1.1')
m_dep = cc.find_library('m', required : false)

lonetix_incdir = include_directories('lonetix/include/df')
lonetix_incdir_local = include_directories('lonetix')

lonetix = static_library('lonetix',
    sources : [
        'lonetix/bgp/attribute.c',
        'lonetix/bgp/bgp.c',
        'lonetix/bgp/bytebuf.c',
        'lonetix/bgp/dump.c',
        'lonetix/bgp/dump_isolario.c',
        'lonetix/bgp/mrt.c',
        'lonetix/bgp/parameters.c',
        'lonetix/bgp/patricia.c',
        'lonetix/bgp/prefix.c',
        'lonetix/bgp/vm_asmtch.c',
        'lonetix/bgp/vm_communities.c',
        'lonetix/bgp/vm.c',
        'lonetix/bgp/vm_dump.c',
        'lonetix/cpr/bzip2.c',
        'lonetix/cpr/flate.c',
        'lonetix/cpr/xz.c',
        'lonetix/sys/con_unix.c',
        'lonetix/sys/dbg_unix.c',
        'lonetix/sys/fs_common.c',
        'lonetix/sys/fs_unix.c',
        'lonetix/sys/ip_common.c',
        'lonetix/sys/ip_unix.c',
        'lonetix/sys/sys_common.c',
        'lonetix/sys/sys_unix.c',
        'lonetix/utf/rune.c',
        'lonetix/utf/runetype.c',
        'lonetix/utf/utflen.c',
        'lonetix/utf/utfrrune.c',
        'lonetix/utf/utfrune.c',
        'lonetix/utf/utfutf.c',
        'lonetix/argv.c',
        'lonetix/bufio.c',
        'lonetix/lexer.c',
        'lonetix/mem.c',
        'lonetix/mem_file.c',
        'lonetix/numlib_atof.c',
        'lonetix/numlib_atoi.c',
        'lonetix/numlib_ftoa.c',
        'lonetix/numlib_itoa.c',
        'lonetix/stm.c',
        'lonetix/stricmp.c',
        'lonetix/strncatz.c',
        'lonetix/strncpyz.c',
        'lonetix/strnicmp.c'
    ],
    c_args : lonetix_args,
    include_directories : [ lonetix_incdir, lonetix_incdir_local ],
    dependencies : [ m_dep, zlib_dep, bz2_dep, lzma_dep ],
    install : true
)
lonetix_dep = declare_dependency(compile_args : lonetix_args,
                                 link_with : lonetix)

# if get_option('build-tests')
# TODO
# endif

# if get_option('build-benchmarks')
# TODO
# endif

doxygen = find_program('doxygen', required : get_option('build-doc'))
if doxygen.found()
    subdir('doc')
elif get_option('build-doc').auto()
    message('Not building documentation as doxygen was not found')
endif

if get_option('build-bgpgrep')
    bgpgrep = executable('bgpgrep',
        sources : [
            'tools/bgpgrep/bgpgrep_asmatch.c',
            'tools/bgpgrep/bgpgrep.c',
            'tools/bgpgrep/bgpgrep_communities.c',
            'tools/bgpgrep/bgpgrep_compile.c',
            'tools/bgpgrep/bgpgrep_dump.c',
            'tools/bgpgrep/bgpgrep_peer.c',
            'tools/bgpgrep/bgpgrep_prefixlist.c',
            'tools/bgpgrep/bgpgrep_timestamp.c',
            'tools/bgpgrep/bgpgrep_vmfunc.c',
        ],
        include_directories : [ lonetix_incdir ],
        dependencies : [ lonetix_dep ],
        install : true
    )

    cdata = configuration_data({
        'UTILITY': 'bgpgrep',
        'VERSION':  '"' + meson.project_version() + '"',
    })

    bgpgrep_man = configure_file(input : 'tools/bgpgrep/bgpgrep.1.in',
                                 output : 'bgpgrep.1',
                                 configuration : cdata,
                                 install : false)

    install_man(bgpgrep_man)
endif
if get_option('build-peerindex')
    peerindex = executable('peerindex',
        sources : [
            'tools/peerindex/peerindex.c'
        ],
        include_directories : [ lonetix_incdir ],
        dependencies : [ lonetix_dep ],
        install : true
    )

    cdata = configuration_data({
        'UTILITY': 'peerindex',
        'VERSION':  '"' + meson.project_version() + '"',
    })

    peerindex_man = configure_file(input : 'tools/peerindex/peerindex.1.in',
                                   output : 'peerindex.1',
                                   configuration : cdata,
                                   install : false)

    install_man(peerindex_man)
endif
if get_option('bgpscanner-legacy')
    supported_systems = [
        'cygwin', 'darwin', 'dragonfly',
        'freebsd', 'gnu', 'linux',
        'netbsd', 'openbsd', 'sunos',
    ]

    if supported_systems.contains(host_machine.system())
        cdata = configuration_data({
            'UTILITY': 'bgpscanner',
            'VERSION': '"' + meson.project_version() + '"',
        })
        bgpscanner_man = configure_file(input : 'tools/bgpscanner/bgpscanner.1.in',
                                        output : 'bgpscanner.1',
                                        configuration : cdata,
                                        install : false)

        install_data('tools/bgpscanner/bgpscanner',
                    install_dir : get_option('bindir'),
                    install_mode : 'rwxr-xr-x')

        install_man(bgpscanner_man)
    else
        message('Skipping bgpscanner legacy wrapper installation on ' + host_machine.system())
    endif
endif
