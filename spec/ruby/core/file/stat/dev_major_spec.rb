require File.dirname(__FILE__) + '/../../../spec_helper'

describe "File::Stat#dev_major" do
  before :each do
    @name = tmp("file.txt")
    touch(@name)
  end
  after :each do
    rm_r @name
  end

  it "returns the major part of File::Stat#dev" do
    File.stat(@name).dev_major.should be_kind_of(Integer)
  end
end
