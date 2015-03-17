/.*chan_dialogicdiva.*/ { next }
/^ *\[global\]/ { global_detected=1; }
/^ *\[modules\]/ { modules_detected=1; }
/.*/ { if (global_detected == 1) { printf "%s\nchan_capi.so=yes\n", $0; global_detected=2; global_updated=1; next; } }
/.*/ { if (modules_detected == 1) { printf "%s\nautoload=yes\nload => chan_capi.so\n", $0; modules_detected=2; modules_updated=1; next; } }
/^ *chan_capi.so *=/ { if (global_detected == 2) { next } }
/^.* *=> *chan_capi.so/ { if (modules_detected == 2) { next } }
/^ *autoload=yes/ { if (modules_detected == 2) { next } }
/^ *\[/ { global_detected=0; modules_detected=0; }
/.*/ { printf "%s\n", $0 }



BEGIN {
  global_detected=0
  modules_detected=0
  global_updated=0
  modules_updated=0
}

END {
  if (modules_updated == 0) { printf "\n[modules]\nautoload=yes\nload => chan_capi.so\n\n"; }
  if (global_updated == 0)  { printf "\n[global]\nchan_capi.so=yes\n\n"; }
}
