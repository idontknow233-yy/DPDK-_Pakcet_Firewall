fs = import('fs')

if fs.exists('cli/acl_cli.c')
  executable(
    'control_plane',
    'cli/acl_cli.c',
    dependencies: dpdk_dep,
    include_directories: include_directories('../dataplane'),
    install: false
  )
endif
