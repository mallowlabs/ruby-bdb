#!/usr/bin/ruby
require 'bdb'

Dir.mkdir 'zippy' unless File.directory? 'zippy'

lockenv = BDB::Env.new('zippy', BDB::INIT_LOCK | BDB::CREATE)
lockid0, lockid1 = lockenv.lock, lockenv.lock

zappy, zippy = "zappy", "zippy"
begin
   lock = lockid0.get(zappy, BDB::LOCK_WRITE)
   begin
      lock = lockid1.get(zappy, BDB::LOCK_WRITE, BDB::LOCK_NOWAIT)
   rescue BDB::LockGranted
      puts "OK must failed"
   rescue
      puts "not OK must give BDB::LockGranted not #$!"
   else
      puts "not OK must failed"
   end
   lock.put
   lock = lockid0.vec [{"op" => BDB::LOCK_GET, "obj" => zappy, 
	 "mode" => BDB::LOCK_WRITE}]
   p lock
   lock = lockid0.vec [{"op" => BDB::LOCK_PUT, "lock" => lock[0]},
      {"op" => BDB::LOCK_GET, "obj" => zippy, "mode" => BDB::LOCK_WRITE}]
   p lock
   lock[1].put
   lockenv.lock_stat.each do |k, v|
      puts "#{k}\t#{v}"
   end
ensure
   lockenv.close
   BDB::Env.remove('./zippy')
end