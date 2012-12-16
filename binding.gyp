{
  'targets': [
    {
      'target_name': 'binding',
      'sources': [ 'binding.cc' ],
      'include_dirs': ['windows/include'],
      'conditions': [
        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              'Delayimp.lib',
            ],
            'conditions': [
              ['target_arch=="ia32"', {
                'libraries': [
                  '<(PRODUCT_DIR)/../../windows/lib/x86/libzmq-v100-mt.lib',
                ]
              },{
                'libraries': [
                  '<(PRODUCT_DIR)/../../windows/lib/x64/libzmq-v100-mt.lib',
                ]
              }]
            ],
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'DelayLoadDLLs': ['libzmq-v100-mt.dll']
            }
          },
        }, {
          'libraries': ['-lzmq'],
          'cflags!': ['-fno-exceptions'],
          'cflags_cc!': ['-fno-exceptions'],
        }],
        ['OS=="mac"', {
          'xcode_settings': {
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES'
          },
          # add macports include & lib dirs
          'include_dirs': [
            '/opt/local/include',
          ],
          'libraries': [
            '-L/opt/local/lib'
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
