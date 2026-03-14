fs = import('fs')

if fs.exists('main.c')
  executable(
    'dpdk_packet_firewall',
    'main.c',
    'acl/acl.c',
    'acl/acl6.c',
    'arp/arp.c',
    'nd/nd.c',
    'route/route.c',
    'route/route6.c',
    'session/session.c',
    'session/session6.c',
    dependencies: dpdk_dep,
    install: false
  )
endif
