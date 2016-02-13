{
  'targets': [
    {
      'target_name': 'zmq',
      'sources': [ 'binding.cc' ],
      'include_dirs' : [
        "<!(node -e \"require('nan')\")"
      ],
      'conditions': [
        ['OS=="win"', {
          'win_delay_load_hook': 'true',
          'include_dirs': ['windows/include'],
          'link_settings': {
            'libraries': [
              'Delayimp.lib',
            ],
            'conditions': [
              ['target_arch=="ia32"', {
                'libraries': [
                  '<(PRODUCT_DIR)/../../windows/lib/x86/libzmq-v100-mt-4_0_4.lib',
                ]
              },{
                'libraries': [
                  '<(PRODUCT_DIR)/../../windows/lib/x64/libzmq-v100-mt-4_0_4.lib',
                ]
              }]
            ],
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'DelayLoadDLLs': ['libzmq-v100-mt-4_0_4.dll']
            }
          },
        }, {
          'libraries': [ '<(PRODUCT_DIR)/../../zmq/lib/libzmq.a' ],
          'include_dirs': [ '<(PRODUCT_DIR)/../../zmq/include' ],
          'cflags!': ['-fno-exceptions'],
          'cflags_cc!': ['-fno-exceptions'],
        }],
        ['OS=="mac" or OS=="solaris"', {
          'xcode_settings': {
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES'
          },
        }],
        ['OS=="openbsd" or OS=="freebsd"', {
        }],
        ['OS=="linux"', {
        }],
      ]
    }
  ]
}
