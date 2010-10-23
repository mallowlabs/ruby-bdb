$:.unshift(File.dirname(__FILE__)) unless
  $:.include?(File.dirname(__FILE__)) || $:.include?(File.expand_path(File.dirname(__FILE__)))

Dir.glob(File.dirname(__FILE__) + "/bdb.*").each do |ext|
  begin
    require ext
  rescue LoadError
  end
end

module BDB
  VERSION = '0.6.5' unless VERSION
end

