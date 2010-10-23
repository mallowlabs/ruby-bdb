require "test/unit"

$:.unshift File.dirname(__FILE__) + "/../ext/bdb"
require "bdb.so"

class TestBdbExtn < Test::Unit::TestCase
  def test_truth
    assert true
  end
end