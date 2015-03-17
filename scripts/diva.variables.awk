/.*<tlie>.*/ { ++state; }
/.*<.tlie>.*/ { --state; start=0; }

/.*<vie id="namedvarname"><as>.*<\/as><\/vie>.*/ {
	if (check_variable == "namedvarname") {
		if (variable_name == get_tag_value($0)) {
			start=1
		}
	}
}

/.*<vie id="pcinitname">.*<\/vie>.*/ {
	if (check_variable == "pcinitname") {
		if (variable_name == get_tag_value($0)) {
			start=1
		}
	}
}

/.*<vie id="raminitoffset">.*<\/vie>.*/ {
	if (check_variable == "raminitoffset") {
		if (variable_name == get_tag_value($0)) {
			start=1
		}
	}
}

/.*/ { if (start == 1) {
				 printf ("%s\n", $0);
			 }
}

BEGIN {
	start=0
	state=0
}

function get_tag_value(tag_str)
{
  gsub(/<.*\">/,"",tag_str);
  gsub(/<\/vie>/,"",tag_str);
  gsub(/\t*/,"",tag_str);
  gsub(/ */,"",tag_str);

  gsub(/^<as>/,"",tag_str);
  gsub(/<\/as>$/,"",tag_str);

  return tag_str;
}

