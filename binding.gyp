{
    "target_defaults": {
      "include_dirs": [
        "build/include",
        "<(node_root_dir)/deps/openssl/openssl/include",
        "/opt/local/include",
        "<!(node -e \"require('nan')\")"
      ]
    },
    "targets": [
    {
      "target_name": "binding",
      "defines": [
        "<!@(export PKG_CONFIG_PATH=`pwd`/build/lib/pkgconfig; if which pkg-config 2>/dev/null 1>&2 && pkg-config --exists MagickWand; then echo USE_WAND; fi)",
      ],
      "libraries": [
        "-L/opt/local/lib",
        "$(shell PKG_CONFIG_PATH=$$(pwd)/lib/pkgconfig pkg-config --silence-errors --static --libs MagickWand)"
      ],
      "sources": [
        "binding.cpp",
      ],
      "conditions": [
        [ 'OS=="mac"', {
          "defines": [
            "OS_MACOSX",
          ],
          "xcode_settings": {
            "OTHER_CFLAGS": [
              "-g -fPIC",
              "$(shell PKG_CONFIG_PATH=$$(pwd)/lib/pkgconfig pkg-config --silence-errors --cflags MagickWand)"
            ],
          },
        }],
        [ 'OS=="linux"', {
          "defines": [
            "OS_LINUX",
          ],
          "cflags_cc+": [
            "-g -fPIC -rdynamic",
            "$(shell PKG_CONFIG_PATH=$$(pwd)/lib/pkgconfig pkg-config --silence-errors --cflags MagickWand)",
          ],
        }],
      ]
    }]
}
