require File.dirname(__FILE__) + '/../spec_helper'

describe "The defined? keyword" do
  class LanguageDefinedSpecs
    SomeConst = 5

    attr_accessor :ivar

    def no_args
    end
    def args(x)
    end

    def lvar_defined
      x = 1
      defined?(x)
    end

    def ivar_definition
      defined? @ivar
    end

  end

  class LanguageDefinedSubclass < LanguageDefinedSpecs
    def no_args
      defined?(super)
    end
    def args
      defined?( super() )
    end
  end

  class LanguageDefinedSubclassDM < LanguageDefinedSpecs
    define_method(:no_args) {
      defined?(super)
    }

    define_method(:args) {
      defined?(super())
    }
  end

  class LanguageDefinedSubclassBlock < LanguageDefinedSpecs
    def no_args
      ret = nil
      1.times { ret = defined?(super) }
      ret
    end

    def args
      ret = nil
      1.times { ret = defined?( super() ) }
      ret
    end
  end

  class LanguageDefinedSubclassDMBlock < LanguageDefinedSpecs
    define_method(:no_args) {
      ret = nil
      1.times { ret = defined?(super) }
      ret
    }

    define_method(:args) {
      ret = nil
      1.times { ret = defined?(super()) }
      ret
    }
  end

  module AAA
    self::FOO = 'x' unless defined? self::FOO rescue nil
  end

  it "returns 'method' when defined?(exit) is sent" do
    ret = defined?(exit)
    ret.should == 'method'
  end

  it "returns 'method' when defined?(Kernel.puts) is sent (attribute)" do
    ret = defined?(Kernel.puts)
    ret.should == 'method'
  end

  it "returns nil when defined?(DoesNotExist.puts) is sent" do
    ret = defined?(DoesNotExist.puts)
    ret.should == nil
  end

  it "returns nil when defined?(Kernel.does_not_exist) is sent" do
    ret = defined?(Kernel.does_not_exist)
    ret.should == nil
  end

  it "returns 'assignment' when defined?(x = 2) is sent" do
    ret = defined?(x = 2)
    ret.should == 'assignment'
  end

  it "returns 'local-variable' when x = 1; defined?(x) is sent" do
    obj = LanguageDefinedSpecs.new
    obj.lvar_defined.should == 'local-variable'
  end

  it "returns 'constant' when defined?(Object) is sent" do
    ret = defined?(Object)
    ret.should == 'constant'
  end

  it "returns 'class variable' when @@x = 1; defined?(@@x) is sent" do
    @@x = 1
    ret = defined?(@@x)
    ret.should == 'class variable'
  end

  it "returns 'instance-variable' when @x = 1; defined?(@x) is sent" do
    @x = 1
    ret = defined?(@x)
    ret.should == 'instance-variable'
  end

  it "returns 'global-variable' when $x = 1; defined?($x) is sent" do
    $x = 1
    ret = defined?($x)
    ret.should == 'global-variable'
  end

  it "returns 'expression' when defined?('foo = bar') is sent" do
    ret = defined?('foo = bar')
    ret.should == "expression"
  end

  it "returns 'self' when defined?(self) is sent" do
    ret = defined?(self)
    ret.should == "self"
  end

  it "returns 'nil' when defined?(nil) is sent" do
    ret = defined?(nil)
    ret.should == "nil"
  end

  it "returns 'true' when defined?(true) is sent" do
    ret = defined?(true)
    ret.should == "true"
  end

  it "returns 'false' when defined?(false) is sent" do
    ret = defined?(false)
    ret.should == "false"
  end

  it "returns nil when defined?(no_such_local) is sent" do
    ret = defined?(no_such_local)
    ret.should == nil
  end

  it "returns 'expression' when defined?(:File) is sent" do
    ret = defined?(:File)
    ret.should == "expression"
  end

  it "returns 'constant' when defined?(LanguageDefinedSpecs::SomeConst) is sent" do
    ret = defined?(LanguageDefinedSpecs::SomeConst)
    ret.should == "constant"
  end
  
  it "returns nil when defined?(LanguageDefinedSpecs::MissingConst) is sent" do
    ret = defined?(LanguageDefinedSpecs::MissingConst)
    ret.should == nil
  end

  it "returns nil when defined?(LanguageDefinedSpecs::MissingConst::MissingConst) is sent" do
    ret = defined?(LanguageDefinedSpecs::MissingConst::MissingConst)
    ret.should == nil
  end

  it "returns 'constant' when evaluating self::FOO in module AAA" do
    ret = defined?(AAA::FOO)
    ret.should == 'constant'
  end

  it "returns 'constant' when evaluating self::FOO in subclass's metaclass" do
    o = Object.new
    class << o
      class Foo; Baz = 1; end
      class Bar < Foo; def self.baz_defined?; defined? self::Baz; end; end
      def bar; Bar; end
    end
    o.bar.baz_defined?.should == 'constant'
  end

  it "returns 'constant' when defined?(File) is sent" do
    ret = defined?(File)
    ret.should == "constant"
  end

  it "returns 'constant' when defined?(::File) is sent" do
    ret = defined?(::File)
    ret.should == "constant"
  end

  it "returns 'constant' when defined?(File::SEPARATOR) is sent" do
    ret = defined?(File::SEPARATOR)
    ret.should == "constant"
  end

  it "returns 'method' when defined?(Object.nil?) is sent" do
    ret = defined?(Object.nil?)
    ret.should == "method"
  end

  it "returns 'expression' when defined?(0) is sent" do
    ret = defined?(0)
    ret.should == "expression"
  end

  it "returns 'yield' when there is a block to yield to and defined?(yield) is sent" do
    def y
      ret = defined?(yield)
      ret.should == 'yield'
    end
    y {|a| true}
  end

  deviates_on :rubinius do
    # Rubinius does not care about dynamic vars
    it "returns 'local-variable' when defined? is called on a block var" do
      block = Proc.new { |x| defined?(x) }
      ret = block.call(1)
      ret.should == 'local-variable'
    end
  end

  it "returns 'super' when Subclass#no_args uses defined?" do
    ret = (LanguageDefinedSubclass.new.no_args)
    ret.should == "super"
  end

  it "returns 'super' when Subclass#args uses defined?" do
    ret = (LanguageDefinedSubclass.new.args)
    ret.should == "super"
  end

  it "returns 'super' when Subclass#no_args and created with define_method" do
    ret = (LanguageDefinedSubclassDM.new.no_args)
    ret.should == "super"
  end

  it "returns 'super' when Subclass#args and created with define_method" do
    ret = (LanguageDefinedSubclassDM.new.args)
    ret.should == "super"
  end

  it "returns 'super' when Subclass#no_args uses a block" do
    ret = (LanguageDefinedSubclassBlock.new.no_args)
    ret.should == "super"
  end

  it "returns 'super' when Subclass#args uses a block" do
    ret = (LanguageDefinedSubclassBlock.new.args)
    ret.should == "super"
  end

  it "returns 'super' when Subclass#no_args uses a block in define_method" do
    ret = (LanguageDefinedSubclassDMBlock.new.no_args)
    ret.should == "super"
  end

  it "returns 'super' when Subclass#args uses a block in define_method" do
    ret = (LanguageDefinedSubclassDMBlock.new.args)
    ret.should == "super"
  end

  not_compliant_on :rubinius do
    ruby_version_is "" ... "1.9" do
      it "returns 'local-variable(in-block)' when defined? is called on a block var" do
        block = Proc.new { |xxx| defined?(xxx) }
        ret = block.call(1)
        ret.should == 'local-variable(in-block)'
      end
    end
  end

  ruby_version_is "1.9" do
    it "returns 'local-variable' when defined? is called on a block var" do
      block = Proc.new { |xxx| defined?(xxx) }
      ret = block.call(1)
      ret.should == 'local-variable'
    end
  end

  it "returns nil for an instance variable not yet accessed" do
    instance = LanguageDefinedSpecs.new
    ret = instance.ivar_definition
    ret.should == nil
  end

  it "returns nil for an instance variable read but not yet assigned" do
    instance = LanguageDefinedSpecs.new
    read_value = instance.ivar
    ret = instance.ivar_definition
    ret.should == nil
  end

  it "returns 'instance-variable' for an instance variable defined in an instance" do
    instance = LanguageDefinedSpecs.new
    instance.ivar = 2
    ret = instance.ivar_definition
    ret.should == 'instance-variable'
  end

  it "follows normal lexical and hierarchical scoping for constants" do
    o = Object.new
    class << o
      module Foo
        Bar = 1
      end
      class Baz; include Foo; end
      class Baz;
        def self.foo_defined; defined? Foo; end
        def self.bar_defined; defined? Bar; end
      end
      def baz; Baz; end
    end

    o.baz.foo_defined.should == "constant";
    o.baz.bar_defined.should == "constant";
  end
end

