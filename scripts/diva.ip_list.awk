#/^(\w+).*/ { name = $1; }
/^([a-zA-Z0-9_]+).*/ { name = $1; }
/.*inet addr:[0-9.]+ .*/ { 
  match($0, /inet addr:[0-9.]+ /); 
  ip = substr($0, RSTART+10, RLENGTH-11); 
  printf "%s\t%s\n", name, ip; 
}
/^$/ {
  name = "";
  ip = "";
}
