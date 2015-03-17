/.*<Name>.*<\/Name>.*/ { 
  sub(/.*<Name>/, ""); 
  sub(/<\/Name>.*/, ""); 
  if (pfad == ".Config.Trace.Profile") { 
    trace_name = $0; 
  } else if (pfad == ".Config.Trace.Profile.Mask") {
    mask_name = $0;
  } else if (pfad == ".Config.Trace.CustomMask.Entry") {
    custom_entry_name = $0;
  }
  next; 
}
/.*<Shortcut>.*<\/Shortcut>.*/ { 
  sub(/.*<Shortcut>/, ""); 
  sub(/<\/Shortcut>.*/, ""); 
  if (pfad == ".Config.Trace.Profile") { 
    trace_short = $0; 
  } else if (pfad == ".Config.Trace.CustomMask.Entry") {
    custom_entry_short = $0;
  }
  next; 
}
/.*<Required>.*<\/Required>.*/ { 
  sub(/.*<Required>/, ""); 
  sub(/<\/Required>.*/, ""); 
  if (pfad == ".Config.Trace.Profile") { 
    trace_required = $0; 
  } else if (pfad == ".Config.Trace.CustomMask") {
    custom_required = $0;
  }
  next; 
}
/.*<Value>.*<\/Value>.*/ { 
  sub(/.*<Value>/, ""); 
  sub(/<\/Value>.*/, ""); 
  if (pfad == ".Config.Trace.Profile.Mask") { 
    mask_value = $0; 
  } else if (pfad == ".Config.Trace.CustomMask.Entry") {
    custom_entry_value = $0;
  }
  next; 
}
/.*<DriverDesc>.*<\/DriverDesc>.*/ { sub(/.*<DriverDesc>/, ""); sub(/<\/DriverDesc>.*/, ""); if (pfad == ".Config.Trace.CustomMask") { custom_driver = $0; } next; }
/.*<DriverName>.*<\/DriverName>.*/ { sub(/.*<DriverName>/, ""); sub(/<\/DriverName>.*/, ""); if (pfad == ".Config.Trace.CustomMask") { custom_name = $0; } next; }
/.*<.*>.*<\/.*>.*/ { next; }
/.*<[A-Z]+.*>.*/ { sub(/.*</, ""); sub(/>.*/, ""); pfad=pfad "." $0; next; }
/.*<\/.*>.*/ {
  sub(/.*<\//, ""); 
  sub(/>.*/, "");
  sub(/\.[A-Za-z0-9]*$/, "", pfad); 
  if ($0 == "Profile" && trace_name != "" && trace_short != "") {
    printf "trace_name[%s]='%s'\n", trace_nr, trace_name;
    printf "trace_short[%s]='%s'\n", trace_nr, trace_short;
    printf "trace_required[%s]='%s'\n", trace_nr, trace_required;
    trace_nr++;
    trace_name = "";
    trace_short = "";
    trace_required = "";
  }
  if ($0 == "Mask" && trace_short != "" && mask_value != "" && mask_name != "") {
    printf "mask_short[%s]='%s'\n", mask_nr, trace_short;
    printf "mask_name[%s]='%s'\n", mask_nr, mask_name;
    printf "mask_value[%s]='%s'\n", mask_nr, mask_value;
    mask_nr++;
    mask_value = "";
    mask_name = "";
  }
  if ($0 == "CustomMask" && custom_driver != "" && custom_name != "") {
    printf "custom_driver[%s]='%s'\n", custom_nr, custom_driver;
    printf "custom_name[%s]='%s'\n", custom_nr, custom_name;
    printf "custom_required[%s]='%s'\n", custom_nr, custom_required;
    custom_nr++;
    custom_driver = "";
    custom_required = "";
    custom_name = "";
  }
  if ($0 == "Entry" && custom_entry_name != "" && custom_entry_value != "" && custom_entry_short != "" && custom_driver != "") {
    printf "custom_entry_driver[%s]='%s'\n", custom_entry_nr, custom_name;
    printf "custom_entry_name[%s]='%s'\n", custom_entry_nr, custom_entry_name;
    printf "custom_entry_short[%s]='%s'\n", custom_entry_nr, custom_entry_short;
    printf "custom_entry_value[%s]='%s'\n", custom_entry_nr, custom_entry_value;
    custom_entry_nr++;
    custom_entry_name = "";
    custom_entry_short = "";
    custom_entry_value = "";
  }
  next; 
}
BEGIN { 
  trace_nr = 0;
  mask_nr = 0;
  custom_nr = 0;
  custom_entry_nr = 0;
}
