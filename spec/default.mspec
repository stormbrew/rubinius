require 'spec/custom/utils/script'

class MSpecScript
  # Language features specs
  set :language, [
    'spec/ruby/language',
    'spec/language'
  ]

  # Core library specs
  set :core, [
    'spec/ruby/core',
    'spec/core',

    # 1.9 syntax (TODO: remove)
    '^core/proc/parameters_spec.rb',

    # 1.8
    '^core/continuation'
  ]

  set :obsolete_library, [
    # obsolete libraries
    '^library/cgi-lib',
    '^library/date2',
    '^library/enumerator',
    '^library/eregex',
    '^library/finalize',
    '^library/ftools',
    '^library/generator',
    '^library/getopts',
    '^library/importenv',
    '^library/jcode',
    '^library/mailread',
    '^library/parsearg',
    '^library/parsedate',
    '^library/ping',
    '^library/readbytes',
    '^library/rubyunit',
    '^library/runit',
    '^library/soap',
    '^library/wsdl',
    '^library/xsd',
    '^library/Win32API',

    '^library/test/unit/collector',
    '^library/test/unit/ui',
    '^library/test/unit/util',

    '^library/dl',  # reimplemented and API changed
  ]

  # Standard library specs
  set :library, [
    'spec/ruby/library',
    'spec/library',
  ] + get(:obsolete_library)

  set :capi, [
    'spec/capi',
    '^spec/capi/globals',
    '^spec/capi/module',
    '^spec/capi/proc',
    '^spec/capi/struct'
  ]

  set :command_line, [ 'spec/command_line' ]

  set :compiler, [ 'spec/compiler' ]

  set :build, [ 'spec/build' ]

  # An ordered list of the directories containing specs to run
  set :files, get(:language) + get(:core) + get(:library) +
              get(:capi) + get(:compiler) + get(:build)

  set :ruby, [
    'spec/ruby/language',
    'spec/ruby/core',
    'spec/ruby/library',
  ] + get(:obsolete_library)

  # An ordered list of the directories containing specs to run
  # as the CI process.
  set :ci_files, [
    'spec/ruby/core',
    'spec/ruby/language',
    'spec/core',
    'spec/compiler',
    'spec/command_line',
    'spec/capi',
    'spec/ruby/library/digest',

    'spec/build',

    '^spec/core/compiledmethod',
    '^spec/core/module',
    '^spec/capi/globals',
    '^spec/capi/module',
    '^spec/capi/proc',
    '^spec/capi/struct',

    # These additional directories will be enabled as the
    # specs in them are updated for the C++ VM.
    # 'spec/debugger',
  ]

  # The set of substitutions to transform a spec filename
  # into a tag filename.
  set :tags_patterns, [
                        [%r(spec/), 'spec/tags/'],
                        [/_spec.rb$/, '_tags.txt']
                      ]

  # Prepended to file names when resolving spec files. Enables
  # commands like 'bin/mspec core/array' to be equivalent to
  # 'bin/mspec spec/ruby/core/array'
  set :prefix, 'spec/ruby'

  # The default implementation to run the specs.
  set :target, 'bin/rbx'

  # Leave out MSpec lines from backtraces
  set :backtrace_filter, %r[(mspec/bin|mspec/lib/mspec)]
end
