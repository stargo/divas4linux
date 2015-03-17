/.*<tlie>.*/ { ++state; }
/.*<.tlie>.*/ { --state; if (state <= 1) owner_detected=0; if (state <= 2) instance_detected=0; }
/.*<vie id="owner">.*<\/vie>.*/ { if (state == 2) {
																		tmp=get_tag_value($0);
																		if (tmp == owner) {
																			owner_detected=1
																		}
																	}
																}
/.*<vie id="instance.*">.*<\/vie>.*/ { if (owner_detected == 1 && state == 3) {
																				tmp=get_tag_value($0);
																				if (tmp == instance) {
																					instance_detected=1;
																					ignore_line=1;
																				}
																			 }
																		 }
!/^#.*/ && ! /^ *$/ { if (owner_detected == 1 && instance_detected == 1) {
				if (ignore_line == 0) {
					printf "%s\n", $0
				}
				ignore_line=0
			 }
		 }


BEGIN {
	state=0
	owner_detected=0
	instance_detected=0
	ignore_line=0
}

function get_tag_value(tag_str)
{
  gsub(/<.*\">/,"",tag_str);
  gsub(/<\/vie>/,"",tag_str);
  gsub(/\t*/,"",tag_str);
  gsub(/ */,"",tag_str);

  return tag_str;
}
