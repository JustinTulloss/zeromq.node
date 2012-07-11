{
  'targets': [
    {
      'target_name': 'binding',
      'sources': [ 'binding.cc' ],
      'conditions': [
        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              '<(PRODUCT_DIR)/../../win32/lib/libzmq-v100-mt.lib',
              'Delayimp.lib',
            ],
          },
          'msvs_settings': {
            'VCLinkerTool': {
              'DelayLoadDLLs': ['libzmq-v100-mt.dll']
            }
          },
          'include_dirs': ['win32/include'],
        }, {
          'libraries': ['-lzmq'],
          'cflags!': ['-fno-exceptions'],
          'cflags_cc!': ['-fno-exceptions'],
        }],
        ['OS=="mac"', {
          'xcode_settings': {
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES'
          }
        }]
      ]
    }
  ]
}
