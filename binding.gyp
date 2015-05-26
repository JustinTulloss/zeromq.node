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
          'libraries': ['-lzmq'],
          'cflags!': ['-fno-exceptions'],
          'cflags_cc!': ['-fno-exceptions'],
        }],
        ['OS=="mac" or OS=="solaris"', {
          'xcode_settings': {
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES'
          },
          # add macports include & lib dirs, homebrew include & lib dirs
          'include_dirs': [
            '<!@(pkg-config libzmq --cflags-only-I | sed s/-I//g)',
            '/opt/local/include',
            '/usr/local/include',
          ],
          'libraries': [
            '<!@(pkg-config libzmq --libs)',
            '-L/opt/local/lib',
            '-L/usr/local/lib',
          ]
        }],
        ['OS=="openbsd" or OS=="freebsd"', {
          'include_dirs': [
            '<!@(pkg-config libzmq --cflags-only-I | sed s/-I//g)',
            '/usr/local/include',
          ],
          'libraries': [
            '<!@(pkg-config libzmq --libs)',
            '-L/usr/local/lib',
          ]
        }],
        ['OS=="linux"', {
          'cflags': [
            '<!(pkg-config libzmq --cflags 2>/dev/null || echo "")',
          ],
          'libraries': [
            '<!(pkg-config libzmq --libs 2>/dev/null || echo "")',
          ],
        }],
      ]
    }
  ]
}
