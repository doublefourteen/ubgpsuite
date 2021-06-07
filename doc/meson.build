cdata = configuration_data({
    'TOP_SRCDIR':   meson.source_root(),
    'TOP_BUILDDIR': meson.build_root(),
    'OUTPUT_DIR':   meson.build_root() / 'doc',
    'VERSION':      meson.project_version(),
    'PROJECT_NAME': meson.project_name(),
})

doxyfile = configure_file(input : 'Doxyfile.in',
                          output : 'Doxyfile',
                          configuration : cdata,
                          install : false)

doc_target = custom_target('doc',
                           build_by_default : false,
                           build_always_stale : true,
                           console : true,
                           command : [ doxygen, doxyfile ],
                           output : [ 'doc' ])

alias_target('doc', doc_target)
