{
    'targets': [
        {
            'target_name': 'mruby',
            'sources': [
                './src/mrb.cc',
                './src/node_js_api.cc',
                './src/node_object.cc',
                './src/node_function.cc',
                './src/value_container.cc',
                './src/convert.cc',
                './src/method.cc'
            ],
            'include_dirs': [
                  'vendor/mruby/include/'
            ],
            'dependencies': [
                'libmruby'
            ],
            'libraries': [
                '-lmruby'
            ],
            'cflags!': [ '-fno-exceptions' ],
            'cflags_cc!': [ '-fno-exceptions' ],
            'conditions': [
                ['OS=="mac"', {
                    'xcode_settings': {
                        'OTHER_LDFLAGS': [
                        ],
                        'OTHER_CFLAGS': [
                        ]
                    },
                }],
            ],
            'ldflags': [
                '-L /home/tokuhirom/dev/node-mruby/vendor/mruby/lib/'
            ],
            'cflags': [
                '-g',
              '-O0'
            ]
        },
        {
            'target_name': 'libmruby',
            'type': 'none',
            'actions': [
              {
                'action_name': 'test',
                'inputs': [''],
                'outputs': [''],
                'conditions': [
                  ['OS=="win"', {
                  'action': [
                      'echo', 'test'
                    ]
                  }, {
                    'action': [
                      'sh', 'mrb-build.sh'
                  ]
                }]
                ]
              }
          ]
        }
    ]
}
