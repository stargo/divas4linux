
/.*<tlie>.*/ { ++state; }
/.*<.tlie>.*/ { --state;
								if (state==1) {
									if (owner == "0x3") {
										nr_xdi_responses++;
										printf "diva_xdi_response_instance[%s]=\"%s\"\n", nr_xdi_responses, 0
										printf "diva_xdi_response_value[%s]=\"%s\"\n", nr_xdi_responses, response
										printf "diva_xdi_response_adapter_name[%s]='%s'\n", nr_xdi_responses, "Diva CAPI driver"
										printf "diva_xdi_response_owner[%s]='%s'\n", nr_xdi_responses, owner
									} else if (owner == "0x6") {
										nr_xdi_responses++;
										printf "diva_xdi_response_instance[%s]=\"%s\"\n", nr_xdi_responses, 0
										printf "diva_xdi_response_value[%s]=\"%s\"\n", nr_xdi_responses, response
										printf "diva_xdi_response_adapter_name[%s]='%s'\n", nr_xdi_responses, "Diva TTY driver"
										printf "diva_xdi_response_owner[%s]='%s'\n", nr_xdi_responses, owner
									} else if (owner == "0xa") {
										nr_xdi_responses++;
										printf "diva_xdi_response_instance[%s]=\"%s\"\n", nr_xdi_responses, 0
										printf "diva_xdi_response_value[%s]=\"%s\"\n", nr_xdi_responses, response
										printf "diva_xdi_response_adapter_name[%s]='%s'\n", nr_xdi_responses, "Diva M-Adapter driver"
										printf "diva_xdi_response_owner[%s]='%s'\n", nr_xdi_responses, owner
									} else if (owner == "0xb") {
										nr_xdi_responses++;
										printf "diva_xdi_response_instance[%s]=\"%s\"\n", nr_xdi_responses, 0
										printf "diva_xdi_response_value[%s]=\"%s\"\n", nr_xdi_responses, response
										printf "diva_xdi_response_adapter_name[%s]='%s'\n", nr_xdi_responses, "Diva softIP adapter"
										printf "diva_xdi_response_owner[%s]='%s'\n", nr_xdi_responses, owner
									} else if (owner == "0xc") {
										nr_xdi_responses++;
										printf "diva_xdi_response_instance[%s]=\"%s\"\n", nr_xdi_responses, 0
										printf "diva_xdi_response_value[%s]=\"%s\"\n", nr_xdi_responses, response
										printf "diva_xdi_response_adapter_name[%s]='%s'\n", nr_xdi_responses, "Diva softIP stack"
										printf "diva_xdi_response_owner[%s]='%s'\n", nr_xdi_responses, owner
									} else if (owner == "0xd") {
										nr_xdi_responses++;
										printf "diva_xdi_response_instance[%s]=\"%s\"\n", nr_xdi_responses, 0
										printf "diva_xdi_response_value[%s]=\"%s\"\n", nr_xdi_responses, response
										printf "diva_xdi_response_adapter_name[%s]='%s'\n", nr_xdi_responses, "Diva SIPcontrol"
										printf "diva_xdi_response_owner[%s]='%s'\n", nr_xdi_responses, owner
									}
									owner=0;instance="";response="";
								}
							}
/.*<vie id="owner">2<\/vie>.*/ { if (state < 4) { owner=get_tag_value($0); instance=""; response=""; } }
/.*<vie id="owner">3<\/vie>.*/ { if (state < 4) { owner=get_tag_value($0); instance=""; response=""; } }
/.*<vie id="owner">6<\/vie>.*/ { if (state < 4) { owner=get_tag_value($0); instance=""; response=""; } }
/.*<vie id="owner">a<\/vie>.*/ { if (state < 4) { owner=get_tag_value($0); instance=""; response=""; } }
/.*<vie id="owner">b<\/vie>.*/ { if (state < 4) { owner=get_tag_value($0); instance=""; response=""; } }
/.*<vie id="owner">c<\/vie>.*/ { if (state < 4) { owner=get_tag_value($0); instance=""; response=""; } }
/.*<vie id="owner">d<\/vie>.*/ { if (state < 4) { owner=get_tag_value($0); instance=""; response=""; } }
/.*instance.*/ { if (state < 4) {
									if (owner=="0x2") { instance=get_tag_value($0); }
								} }
/.*response.*/ { if (state < 4) {
									if (owner=="0x2") { response=get_tag_value($0); output_response(owner,instance,response);
                                  instance=""; response=""; }
									if (owner=="0x3") { if (get_tag_value($0) != "0x0") { response=3 } else { if (response != 3) response=0 } }
									if (owner=="0x6") { if (get_tag_value($0) != "0x0") { response=3 } else { if (response != 3) response=0 } }
									if (owner=="0xa") { if (get_tag_value($0) != "0x0") { response=3 } else { if (response != 3) response=0 } }
									if (owner=="0xb") { if (get_tag_value($0) != "0x0") { response=3 } else { if (response != 3) response=0 } }
									if (owner=="0xc") { if (get_tag_value($0) != "0x0") { response=3 } else { if (response != 3) response=0 } }
									if (owner=="0xd") { if (get_tag_value($0) != "0x0") { response=3 } else { if (response != 3) response=0 } }
							   }
							 }
/.*<vie id="namedvarname"><as>name<\/as><\/vie>.*/ {
																	if (owner=="0x2") { read_adapter_name=1; adapter_name=""; }
																										}
/.*<vie id="namedvarname"><as>serial<\/as><\/vie>.*/ {
																	if (owner=="0x2") { read_adapter_serial_number=1; adapter_serial_number=""; }
																										}
/.*<vie id="namedvarname"><as>adapters<\/as><\/vie>.*/ {
																	if (owner=="0x2") { read_adapters=1; adapters=-1; }
																												}
/.*<vie id="namedvarname"><as>adapter<\/as><\/vie>.*/ {
																	if (owner=="0x2") { read_adapter=1; adapter=-1; }
																											}
/.*namedvarvalue.*/ {
											if (owner == "0x2") {
												if (read_adapter_name == 1) {
            	            adapter_name=get_namedvar_value($0);
													read_adapter_name=0;
												} else if (read_adapter_serial_number == 1) {
                        	read_adapter_serial_number=0;
                        	adapter_serial_number=get_namedvar_value($0);
                      	} else if (read_adapters == 1) {
                        	read_adapters=0;
                        	adapters=get_namedvar_value($0);
                      	} else if (read_adapter == 1) {
                        	read_adapter=0;
                        	adapter=get_namedvar_value($0);
                      	}
											}
                    }

BEGIN {
 state=0
 owner=""
 instance=""
 response=""

 read_adapter_name=0
 adapter_name=""

 read_adapter_serial_number=0
 adapter_serial_number=""

 read_adapters=0
 adapters=-1

 read_adapter=0
 adapter=-1

 nr_xdi_responses=0
}

END {
  printf "diva_xdi_response_nr=\"%d\"\n", nr_xdi_responses
}

function get_tag_value(tag_str)
{
  gsub(/<.*\">/,"",tag_str);
  gsub(/<\/vie>/,"",tag_str);
  gsub(/\t*/,"",tag_str);
  gsub(/ */,"",tag_str);

	tag_str=sprintf("0x%s", tag_str);

  return tag_str;
}

function get_namedvar_value(tag_str)
{
  gsub(/^.*\">/,"",tag_str);
  gsub(/<as>/,"",tag_str);
  gsub(/<\/as>/,"",tag_str);
  gsub(/<bs>/,"",tag_str);
  gsub(/<\/bs>/,"",tag_str);
  gsub(/<\/vie>.*$/,"",tag_str);

	return(tag_str)
}


function output_response(section_owner,section_instance,section_response,     adapter_port_name)
{
	if (section_owner == "0x2") {
      nr_xdi_responses++;
      printf "diva_xdi_response_instance[%s]=\"%s\"\n", nr_xdi_responses, section_instance
      printf "diva_xdi_response_value[%s]=\"%s\"\n", nr_xdi_responses, section_response
      printf "diva_xdi_response_owner[%s]='%s'\n", nr_xdi_responses, "0x2"

      if (adapters > 1) {
        adapter++;
      }
      if (adapter > 0) {
        adapter_port_name = sprintf("PORT: %d ", adapter);
      } else {
        adapter_port_name = "";
      }

      printf "diva_xdi_response_adapter_name[%s]='%s %sSN:%s'\n", nr_xdi_responses, adapter_name, adapter_port_name, adapter_serial_number

      read_adapter_name=0
      read_adapter_serial_number=0
      read_adapters=0
      read_adapter=0
      adapter=-1
	}
}

