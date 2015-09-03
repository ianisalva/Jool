#include "nat64/common/JsonReader.h"

static int do_parsing(char * buffer);
static int parse_nat64_json(cJSON * json_structure);
static int parse_nat64_global(cJSON * global_json);
static int parse_nat64_pool6(cJSON * pool6_json);
static int parse_nat64_pool4(cJSON * pool4_json);
static int parse_nat64_bib(cJSON * bib_json);

static int send_buffers();
static int send_global_buffer();
static int send_pool6_buffer();
static int send_pool4_buffer();
static int send_bib_buffer();
static int send_multipart_request_buffer(__u8*buffer,
		__u16 request_len, __u16 section);


#ifdef DEBUG

static int print_config();
static int print_global();
static int print_pool6();
static int print_pool4();
static int print_bib();

#endif



static union global_bits * configured_parameters;
static __u16 num_items_mtu_plateaus;
static struct global_config * global;


static struct ipv6_prefix * pool6_entry;
static __u16 pool4_entries_num = 0;
static __u8 * pool4_buffer;
static __u16 bib_entries_num = 0;
static __u8 * bib_buffer;


int parse_file(char * fileName) {
	FILE * file = fopen(fileName, "rb");

	long length;
	long read_bytes = 0;
	char * buffer = 0;

	int error = 0;

	if (file) {
		fseek(file, 0, SEEK_END);
		length = ftell(file);
		fseek(file, 0, SEEK_SET);
		buffer = malloc(length);
		if (buffer) {

		   while(read_bytes < length)
		   {
		      read_bytes+= fread(&buffer[read_bytes], 1, length, file);
		   }
		}
		fclose(file);
	} else {
		printf("%s", "File not found!");
		error = -1;
	}

	if (buffer) {
		error = do_parsing(buffer);
	} else {
		printf("%s", "No buffer!");
		error = -1;
	}

	return error;
}

static int do_parsing(char * buffer) {
	int error = 0;

	cJSON * json_structure = cJSON_Parse(buffer);

	if (json_structure) {

		cJSON * file_type = cJSON_GetObjectItem(json_structure, "File_Type");

		if (file_type) {
			if (strcmp(file_type->valuestring, "NAT64") != 0) {
				printf("File_Type value - %s - is not valid.\n",
						file_type->valuestring);
				return 1;
			}
		}

		error = parse_nat64_json(json_structure);

		if(!error)
		{

			#ifdef DEBUG
			error = print_config();
			if(error)
			{
				log_err("Something went wrong while trying to print the configuration!.");
				return 1;
			}
			#endif

			error = send_buffers();
			if(error)
			{
				log_err("Something went wrong while trying to send the buffers to the kernel!.") ;
			}

		}

	} else {
		log_err("Something went wrong while trying to parse the file!. ->  %s",
				cJSON_GetErrorPtr());
		return 1;
	}

	return 0;
}

static int parse_nat64_json(cJSON * json_structure) {
	int error = 0;

	cJSON * global = cJSON_GetObjectItem(json_structure, "Global");
	error = parse_nat64_global(global);

	if (error) {
		return error;
	}

	cJSON * pool6 = cJSON_GetObjectItem(json_structure, "Pool6");
	error = parse_nat64_pool6(pool6);

	if (error) {
		return error;
	}

	cJSON * pool4 = cJSON_GetObjectItem(json_structure, "Pool4");
	error = parse_nat64_pool4(pool4);

	if (error) {
		return error;
	}

	cJSON * bib = cJSON_GetObjectItem(json_structure, "BIB");
	error = parse_nat64_bib(bib);

	return error;
}

static int parse_nat64_global(cJSON * global_json)
{
	int error = 0;
	cJSON * read_value;
	__u8 bool_value = 0;

	//We verify that the bitfield structure, that tells which parameters are initialized, dont be already allocated, if so, we free it.
	if(configured_parameters)
	free(configured_parameters);

	configured_parameters  = malloc(sizeof(union global_bits));


	if (global_json) {

		global = (struct global_config *) malloc(sizeof(struct global_config));
		read_value = cJSON_GetObjectItem(global_json, "manually-enabled") ;
		configured_parameters->as_fields.manually_enabled = 0;

		//Se intenta leer el parámetro manually-enabled.
		if(read_value) {

		  error = str_to_bool(read_value->valuestring,&bool_value) ;

		  if(error) {
		   log_err("manually-enabled, not valid!. Global: %s",
				   read_value->valuestring) ;
		   return error;
		  }
           configured_parameters->as_fields.manually_enabled = 1;
		   global->is_disable = !bool_value;
		}

		read_value = cJSON_GetObjectItem(global_json,
				"address-dependent-filtering") ;
		configured_parameters->as_fields.address_dependent_filtering = 0;

		//Se intenta leer el parámetro address-dependent-filtering.
		if(read_value)
		{
            error = str_to_bool(read_value->valuestring,&bool_value) ;

            if(error) {
             log_err("address-dependent-filtering, not valid!. Global: %s",
            		 read_value->valuestring) ;
              return error;
            }
            configured_parameters->as_fields.address_dependent_filtering = 1;
            global->nat64.drop_by_addr = bool_value;

		}

		read_value = cJSON_GetObjectItem(global_json, "drop-icmpv6-info") ;
		configured_parameters->as_fields.drop_icmpv6_info = 0;

		//Se intenta leer el parámetro drop-icmpv6-info.
		if(read_value)
		{
			error = str_to_bool(read_value->valuestring,&bool_value) ;

			if (error) {
			 log_err("drop-icmpv6-info , not valid!. Global: %s",
					 read_value->valuestring);
				 return error;
			 }
			configured_parameters->as_fields.drop_icmpv6_info = 1;
			global->nat64.drop_icmp6_info = bool_value;
		}

		read_value = cJSON_GetObjectItem(global_json,
				"drop-externally-initiated-tcp") ;
		configured_parameters->as_fields.drop_externally_initiated_tcp = 0;

		//Se intenta leer el parámetro drop-externally-initiated-tcp.
		if(read_value)
		{
			error = str_to_bool(read_value->valuestring,&bool_value) ;

		   if (error) {
			log_err("drop-externally-initiated-tcp , not valid!. Global: %s",
					            		read_value->valuestring);
			return error;
		  }
		   configured_parameters->as_fields.drop_externally_initiated_tcp = 1;
		  global->nat64.drop_external_tcp = bool_value;

		}

		read_value = cJSON_GetObjectItem(global_json, "udp-timeout") ;
		configured_parameters->as_fields.udp_timeout = 0;

		__u64 u64_value;

		//Se intenta leer el parámetro udp-timeout.
		if(read_value)
		{
          error = str_to_u64(read_value->valuestring,&u64_value,0,UINT_MAX) ;

		  if (error) {
		    log_err("udp-timeout , not valid!. Global: %s",read_value->valuestring);
		     return error;
		   }
		  configured_parameters->as_fields.udp_timeout = 1;
		   global->nat64.ttl.udp = u64_value;

		}

		read_value = cJSON_GetObjectItem(global_json, "tcp-est-timeout") ;
		configured_parameters->as_fields.tcp_est_timeout = 0;

		//Se intenta leer el parámetro tcp-est-timeout.
		if(read_value) {

			//TODO checar con alberto cuanto sera el máximo y el mínimo
			error = str_to_u64(read_value->valuestring,&u64_value,0,UINT_MAX) ;

			if (error) {
			 log_err("tcp-est-timeout , not valid!. Global: %s",
			            		            read_value->valuestring);
			     return error;
			}
			configured_parameters->as_fields.tcp_est_timeout = 1;
			global->nat64.ttl.tcp_est = u64_value;

		}

		read_value = cJSON_GetObjectItem(global_json, "tcp-trans-timeout") ;
		configured_parameters->as_fields.tcp_trans_timeout = 0;

		//Se intenta leer el parámetro tcp-trans-timeout.
		if(read_value)
		{
			error = str_to_u64(read_value->valuestring,&u64_value,0,UINT_MAX) ;

			if (error) {
			 log_err("tcp-trans-timeout , not valid!. Global: %s",
					 read_value->valuestring);
		     return error;
		   }
			configured_parameters->as_fields.tcp_trans_timeout = 1;
		   global->nat64.ttl.tcp_trans =  u64_value;

		}

		read_value = cJSON_GetObjectItem(global_json, "icmp-timeout") ;
		configured_parameters->as_fields.icmp_timeout = 0;

		//Se intenta leer el parámetro icmp-timeout.
		if(read_value)
		{
		  error = str_to_u64(read_value->valuestring,&u64_value,0,UINT_MAX) ;

		  if (error) {
			  log_err("icmp-timeout , not valid!. Global: %s",
		  	             		              read_value->valuestring);
		   return error;
		  }

		  configured_parameters->as_fields.icmp_timeout = 1;
		  global->nat64.ttl.icmp = u64_value;

		}

		read_value = cJSON_GetObjectItem(global_json,
				"fragment-arrival-timeout") ;
		configured_parameters->as_fields.fragment_arrival_timeout = 0;

		//Se intenta leer el parámetro fragment-arrival-timeout.
		if(read_value)
		{
		    error = str_to_u64(read_value->valuestring,&u64_value,0,UINT_MAX) ;

		    if (error) {
	 	      log_err("fragment-arrival-timeout , not valid!. Global: %s",
		      	             		              read_value->valuestring);
		       return error;
		    }
		    configured_parameters->as_fields.fragment_arrival_timeout = 1;
			global->nat64.ttl.frag = u64_value;

		}

		read_value = cJSON_GetObjectItem(global_json,
				"maximum-simultaneous-opens") ;
		 configured_parameters->as_fields.maximum_simultaneous_opens = 0;

		//Se intenta leer el parámetro maximum-simultaneous-opens.
		if(read_value)
		{
		   error = str_to_u64(read_value->valuestring,&u64_value,0,UINT_MAX) ;

		   if (error) {
		  	 log_err("maximum-simultaneous-opens , not valid!. Global: %s",
		  	     	             		              read_value->valuestring);
		  	 return error;
		   }
		   configured_parameters->as_fields.maximum_simultaneous_opens = 1;
		   global->nat64.max_stored_pkts = u64_value;

		}

		read_value = cJSON_GetObjectItem(global_json,
				"source-icmpv6-errors-better") ;
		configured_parameters->as_fields.source_icmpv6_errors_better = 0;

		//Se intenta leer el parámetro source-icmpv6-errors-better.
		if(read_value)
		{
		   error = str_to_bool(read_value->valuestring,&bool_value) ;

		   if (error) {
		  	 log_err("source-icmpv6-errors-better , not valid!. Global: %s",
		  	 	     	             		           read_value->valuestring);
		  	 return error;
		   }
		   configured_parameters->as_fields.source_icmpv6_errors_better = 1;
		   global->nat64.src_icmp6errs_better = bool_value;

		}

		read_value = cJSON_GetObjectItem(global_json,
				"logging-bib") ;
		configured_parameters->as_fields.logging_bib = 0;

		//Se intenta leer el parámetro logging-bib.
		if(read_value)
		{
		   error = str_to_bool(read_value->valuestring,&bool_value) ;

		   if (error) {
		   	 log_err("logging-bib , not valid!. Global: %s",
		   	 	     	             		      read_value->valuestring);
		   	 return error;
		   }
		   configured_parameters->as_fields.logging_bib = 1;
		   global->nat64.bib_logging = bool_value;

		}

		read_value = cJSON_GetObjectItem(global_json,
				"logging-session") ;
		configured_parameters->as_fields.logging_session = 0;

		//Se intenta leer el parámetro logging-session.
		if(read_value)
		{
		   error = str_to_bool(read_value->valuestring,&bool_value) ;

		   if (error) {
		  	 log_err("logging-session , not valid!. Global: %s",
		  	 	     	             		      read_value->valuestring);
		  	 return error;
		   }
		   configured_parameters->as_fields.logging_session = 1;
		   global->nat64.session_logging = bool_value;

		}

		read_value = cJSON_GetObjectItem(global_json,
				"zeroize-traffic-class") ;
		configured_parameters->as_fields.zeroize_traffic_class = 0;

		//Se intenta leer el parámetro zeroize-traffic-class.
		if(read_value)
		{

		   error = str_to_bool(read_value->valuestring,&bool_value) ;

		   if (error) {
		   log_err("zeroize-traffic-class , not valid!. Global: %s",
		   	     	             		      read_value->valuestring);
		   	 return error;
		   }
		   configured_parameters->as_fields.zeroize_traffic_class = 1;
		   global->reset_traffic_class = bool_value;

		}

		read_value = cJSON_GetObjectItem(global_json,
				"override-tos") ;
		configured_parameters->as_fields.override_tos = 0;

		//Se intenta leer el parámetro override-tos.
		if(read_value)
		{
		   error = str_to_bool(read_value->valuestring,&bool_value) ;

		   if (error) {
		    log_err("override-tos , not valid!. Global: %s",
		   	  	     	             		      read_value->valuestring);
		   	  	 return error;
		   }
		  configured_parameters->as_fields.override_tos = 1;
		  global->reset_tos = bool_value;

		}

		read_value = cJSON_GetObjectItem(global_json,"tos") ;
		configured_parameters->as_fields.tos = 0;
		__u8  u8_value;

		//Se intenta leer el parámetro tos.
		if(read_value)
		{
		   error = str_to_u8(read_value->valuestring,&u8_value,0,255);

		   if (error) {
		    log_err("tos , not valid!. Global: %s",read_value->valuestring);
		   	 return error;
		   }

		   global->new_tos = u8_value;
		   configured_parameters->as_fields.tos = 1;

		}

		read_value = cJSON_GetObjectItem(global_json,
				"mtu-plateaus") ;

		configured_parameters->as_fields.mtu_plateaus = 0;

		int i;
		num_items_mtu_plateaus = 0;

		//Se intenta leer el parámetro mtu-plateaus.
		if(read_value)
		{
           cJSON * mtu_item = read_value->child;


           while(mtu_item)
           {
        	  mtu_item = mtu_item->next;
        	  num_items_mtu_plateaus++;
           }

           global->mtu_plateaus =
        		   malloc(sizeof(__u16)*num_items_mtu_plateaus) ;
           __u16 value;
           mtu_item  = read_value->child;

           for(i=0 ; i < num_items_mtu_plateaus; i++)
           {
        	  error = str_to_u16(mtu_item->valuestring,&value,0,3000) ;

        	  if(error)
        	  {
        		log_err("mtu-plateaus, not valid!. Global: %s",
        				read_value->valuestring) ;

        		return error;
        	  }

		      global->mtu_plateaus[i] = value;
		      mtu_item = mtu_item->next;
           }

              configured_parameters->as_fields.mtu_plateaus = 1;
	   }

     }

  return error;
}

static int parse_nat64_pool6(cJSON * pool6_json) {
  int error = 0;
  //ver cual va a ser el valor por default si es que habra alguno.
  struct ipv6_prefix pool6_value;
  if (pool6_json) {
    pool6_entry = malloc(sizeof(struct ipv6_prefix));

    error = str_to_ipv6_prefix(pool6_json->valuestring,&pool6_value);
    if(!error)
    {
       	pool6_entry->address = pool6_value.address;
        pool6_entry->len = pool6_value.len;
    }
    else
    {
    	log_err("Pool6 value not valid!.: %s",pool6_json->valuestring);
    }

  }

  return error;
}

static int parse_nat64_pool4(cJSON * pool4_json) {
int error = 0;


int num_items = 0;
if (pool4_json) {

	cJSON * item = pool4_json->child;

	while (item) {
		num_items++;
		item = item->next;
	}
    pool4_entries_num = num_items;
	item = pool4_json->child;

	pool4_buffer =  malloc(sizeof(__u8) * num_items);

	int i;


	cJSON * mark;
	__u32 mark_value;

	cJSON * prefix;

	struct ipv4_prefix prefix_value;

	cJSON * port_range;
	struct port_range port_range_value;

	for (i = 0; i < num_items; i++) {



		mark = cJSON_GetObjectItem(item, "mark");
		prefix = cJSON_GetObjectItem(item, "prefix");
		port_range = cJSON_GetObjectItem(item, "port_range");

		mark_value = 0;

		//Se indica automaticamente que el mark no va asignado.
		pool4_buffer[i*16] = 0;


	if (mark) {

		//Se indica que el mark si se asignó.
		 pool4_buffer[i*16] = 1;

		 error = str_to_u32(mark->valuestring, &mark_value,0,3000);

		 if (error) {
			log_err("mark, not valid!. Pool4 item: %d", (i+1) );
			break;
		 }

		 memcpy(&pool4_buffer[(i*16)+1],(__u8*)&mark_value,4);
	}
	pool4_buffer[(i*16)+5] = 0;
	if (port_range) {

		pool4_buffer[(i*16)+5] = 1;
		error = str_to_port_range(port_range->valuestring, &port_range_value);

		if (error) {
			log_err("port_range, not valid!. Pool4 item #%d", (i+1) );
			break;
	    }
		 memcpy(&pool4_buffer[(i*16)+6],&port_range_value.min,2);
		 memcpy(&pool4_buffer[(i*16)+8],&port_range_value.max,2);
	 }


	if(!prefix)
	{
		log_err("Pool4 item #%d does not contain a prefix element.",(i+1));
		return 1;
	}

	pool4_buffer[(i*16)+10] = 1;

	 error = str_to_ipv4_prefix(prefix->valuestring, &prefix_value);

     if(error) {
          log_err("prefix, not valid!. Pool4 item #%d", (i+1)) ;
          break;
     }

     memcpy(&pool4_buffer[(i*16)+11],&prefix_value.address,4);
     pool4_buffer[(i*16)+15] = prefix_value.len;


	 item = item->next;

	}
  }

  return error;
}

static int parse_nat64_bib(cJSON * bib_json) {
int error = 0;
int num_items = 0;
bib_entries_num = 0;

  if (bib_json) {

	cJSON * item = bib_json->child;

	while (item) {
		item = item->next;
		num_items++;
	}
	bib_entries_num = num_items;
	bib_buffer = malloc(sizeof(__u8)*BIB_ENTRY_SIZE*bib_entries_num);

	item = bib_json->child;
	int i;

	cJSON * protocol;
	cJSON * address_ipv4;
	cJSON * address_ipv6;


	__u8 protocol_value;
	struct ipv4_transport_addr ipv4_addr;
	struct ipv6_transport_addr ipv6_addr;

	for (i = 0; i < num_items; i++) {
		protocol = cJSON_GetObjectItem(item, "protocol");
		if (strcmp(protocol->valuestring, "TCP") != 0 &&
				strcmp(protocol->valuestring, "UDP") != 0) {
			log_err("Protocol not valid!. BIB item: %d", (i+1) );
			error = 1;
			break;
		}

		if(strcmp(protocol->valuestring, "TCP") == 0) {
			protocol_value = 0;
		} else {
			protocol_value = 1;
		}


		address_ipv4 = cJSON_GetObjectItem(item, "address_ipv4");

		if(!address_ipv4)
		{
			log_err("BIB item #%d does not contain an address_ipv4 element.",(i+1)) ;
			return 1;
		}

		address_ipv6 = cJSON_GetObjectItem(item, "address_ipv6");

		if(!address_ipv6)
		{
			log_err("BIB item #%d does not contain an address_ipv4 element.", (i+1)) ;
			return 1;
		}


		error = str_to_addr4_port(address_ipv4->valuestring, &ipv4_addr);

		if (error) {
			log_err("Address IPv4, not valid!. BIB item #%d",
					(i+1) );
			break;
		}

		error = str_to_addr6_port(address_ipv6->valuestring, &ipv6_addr);

		if (error) {
			log_err("Address IPv6, not valid!. BIB item #%d ",
					(i+1) );
			break;
		}

		bib_buffer[i*BIB_ENTRY_SIZE] = protocol_value;

		memcpy(&bib_buffer[i*BIB_ENTRY_SIZE+1],&ipv4_addr.l3,4);
		memcpy(&bib_buffer[i*BIB_ENTRY_SIZE+5],&ipv4_addr.l4,2);

		memcpy(&bib_buffer[i*BIB_ENTRY_SIZE+7],&ipv6_addr.l3,16);
		memcpy(&bib_buffer[i*BIB_ENTRY_SIZE+23],&ipv6_addr.l4,2);

		item = item->next;

	}

  }
   return error;
}


static int send_buffers()
{
   int error = 0;

   netlink_init_multipart_connection(0,0) ;
   error = send_multipart_request_buffer(0,0,SEC_INIT) ;

   error = send_global_buffer();

   	   if(error)
   		   goto error_happened;

   	   error = send_pool6_buffer();
   	   if(error)
   		   goto error_happened;

   	   error = send_pool4_buffer();
   	   if(error)
   	   	   goto error_happened;

   	   error = send_bib_buffer();
       if(error)
   	       goto error_happened;


       error = send_multipart_request_buffer(0,0,SEC_DONE) ;
      	   if(error)
 		goto error_happened;
 	  netlink_request_multipart_done();

 	 return 0;

 	   error_happened:
 	   netlink_request_multipart_close();
 	   return error;

}
static int send_global_buffer()
{
   int error = 0;
   int index = 0;

   __u8 global_parameters_buffer[64];
   __u8 plateaus_value[2];



   memcpy(global_parameters_buffer,(__u8*)(&configured_parameters->as_int),4) ;


        index+=4;

   		if(configured_parameters->as_fields.manually_enabled) {
   		   memcpy(&global_parameters_buffer[index],&global->is_disable,1);
   		}

   		index+=1;


   		if(configured_parameters->as_fields.address_dependent_filtering)
   		{
           memcpy(&global_parameters_buffer[index],
        		   &global->nat64.drop_by_addr,1) ;
   		}

   		index+=1;


   		if(configured_parameters->as_fields.drop_icmpv6_info)
   		{

   		  memcpy(&global_parameters_buffer[index],
   				           &global->nat64.drop_icmp6_info,1) ;
   		}

   		index+=1;


   		if(configured_parameters->as_fields.drop_externally_initiated_tcp)
   		{
   		 memcpy(&global_parameters_buffer[index],
   				&global->nat64.drop_external_tcp,1) ;
   		}

   		index+=1;


   		if(configured_parameters->as_fields.udp_timeout)
   		{
   			memcpy(&global_parameters_buffer[index],
   					&global->nat64.ttl.udp,8) ;
   		}

   		index+=8;


   		if(configured_parameters->as_fields.tcp_est_timeout) {

   	    	memcpy(&global_parameters_buffer[index],
   	    			&global->nat64.ttl.tcp_est,8) ;
   		}

   		index+=8;


   		if(configured_parameters->as_fields.tcp_trans_timeout)
   		{
   			memcpy(&global_parameters_buffer[index],
   					&global->nat64.ttl.tcp_trans,8) ;
   		}

   		index+=8;


   		if(configured_parameters->as_fields.icmp_timeout)
   		{
   		   memcpy(&global_parameters_buffer[index],
   				&global->nat64.ttl.icmp,8) ;
   		}

   		index+=8;


   		if(configured_parameters->as_fields.fragment_arrival_timeout)
   		{
  		   memcpy(&global_parameters_buffer[index],
  					&global->nat64.ttl.frag,8) ;
   		}

   		index+=8;


   		if(configured_parameters->as_fields.maximum_simultaneous_opens)
   		{
   		   memcpy(&global_parameters_buffer[index],
   				&global->nat64.max_stored_pkts,8) ;
   		}

   		index+=8;


   		if(configured_parameters->as_fields.source_icmpv6_errors_better)
   		{
   		   memcpy(&global_parameters_buffer[index],
   				&global->nat64.src_icmp6errs_better,1) ;
   		}

   		index+=1;


   		if(configured_parameters->as_fields.logging_bib)
   		{
   		   memcpy(&global_parameters_buffer[index],
   				  &global->nat64.bib_logging,1) ;
   		}

   		index+=1;


   		if(configured_parameters->as_fields.logging_session)
   		{
   		   memcpy(&global_parameters_buffer[index],
   				&global->nat64.session_logging,1) ;
   		}

   		index+=1;


   		if(configured_parameters->as_fields.zeroize_traffic_class)
   		{
  		   memcpy(&global_parameters_buffer[index],
  				 &global->reset_traffic_class,1) ;
   		}

   		index+=1;


   		if(configured_parameters->as_fields.override_tos)
   		{
   			memcpy(&global_parameters_buffer[index],
   					&global->reset_tos,1) ;
   		}

   		index+=1;


   		if(configured_parameters->as_fields.tos)
   		{
   			memcpy(&global_parameters_buffer[index],
   					&global->new_tos,1) ;
   		}

   		index+=1;

   		memcpy(&global_parameters_buffer[index],
   				(__u8*)&num_items_mtu_plateaus,2) ;


   		error = send_multipart_request_buffer(
   				global_parameters_buffer,64,SEC_GLOBAL) ;

   		if(error) {
   	    log_err("Something went wrong when sending Global Parameters"
   	    		" to the kernel!.");
   			return error;
   		}

   		log_info("Global buffer has been sent.");

   		index+=2;
   		int i;

   		log_info("Sending %d mtu-plateaus-items", num_items_mtu_plateaus) ;

   		if(configured_parameters->as_fields.mtu_plateaus)
   		{
           for(i=0 ; i < num_items_mtu_plateaus; i++)
           {
        	  memcpy(plateaus_value,(__u8*)&(global->mtu_plateaus[i]),2) ;
        	  error = send_multipart_request_buffer(plateaus_value,
        	     			2,SEC_GLOBAL) ;

        	  if(error)
        	  {
        	    log_err("Something went wrong when sending a mtu-plateaus"
        		 	    		" entry to the kernel!.");
        		  return error;
        	  }

           }
   		}

   		log_info("Sending mtu-plateaus-items have been sent.") ;


   	   return 0;

  }
static int send_pool6_buffer()
{
   int error = 0;

   error = send_multipart_request_buffer((__u8*)&pool6_entry->address,
		   16,SEC_POOL6) ;

   if(error)
   {
	log_err("Something went wrong while sending the pool6 prefix address to the kernel!.");
	return error;
   }

        error = send_multipart_request_buffer(&pool6_entry->len,
		   1,SEC_POOL6) ;

   if(error)
   {
	log_err("Something went wrong while sending the pool6 prefix segment to the kernel!.");
	return error;
   }

   return error;
}
static int send_pool4_buffer()
{
   int error = 0;

   __u32 page_size = getpagesize();
   __u8 entry_size = POOL4_ENTRY_SIZE;

   __u32 buffer_size = (page_size-sizeof(struct request_hdr)-sizeof(struct nlmsghdr))-100;
   __u32 entries_per_message =  (buffer_size-2)/ entry_size;

   __u16 i;
   __u8 kernel_buffer[buffer_size];
   __u8 * kernel_buffer_pointer;

   error = send_multipart_request_buffer((__u8*)&pool4_entries_num,2,SEC_POOL4) ;

   if(error)
   {
   	log_err("error while sending the pool4 entries number to the kernel!.");
   	return error;
   }
   __u16 items_sent = 0;
   __u16 items_sent_in_message = 0;
   __u16 real_index = 0;
   while(items_sent < pool4_entries_num)
   {
	   kernel_buffer_pointer = kernel_buffer;

	   kernel_buffer_pointer += 2;

	   for(i=0; (i < entries_per_message) && (real_index < pool4_entries_num);i++)
	   {
		   kernel_buffer_pointer[0] =  pool4_buffer[real_index*entry_size];
		   memcpy(&kernel_buffer_pointer[1],&pool4_buffer[real_index*entry_size+1],4);
		   kernel_buffer_pointer[5] = pool4_buffer[real_index*entry_size+5];
		   memcpy(&kernel_buffer_pointer[6],&pool4_buffer[real_index*entry_size+6],2);
		   memcpy(&kernel_buffer_pointer[8],&pool4_buffer[real_index*entry_size+8],2);
		   kernel_buffer_pointer[10] = pool4_buffer[(i*16)+10];
		   memcpy(&kernel_buffer_pointer[11],&pool4_buffer[real_index*entry_size+11],4);
		   kernel_buffer_pointer[15] = pool4_buffer[(i*16)+15];

		   kernel_buffer_pointer+=entry_size;
		   real_index++;
	   }

	   items_sent_in_message = real_index - items_sent;

	   memcpy(kernel_buffer,(__u8*)&items_sent_in_message,2);
	   	items_sent = real_index;

	   error = send_multipart_request_buffer(kernel_buffer,buffer_size,SEC_POOL4) ;

	   if(error) {
	     log_err("error while sending a pool4 entry to the kernel!.");
	     break;
	  }

   }

   return error;
}
static int send_bib_buffer()
{
   int error = 0;

   int page_size = getpagesize();
   int entry_size = BIB_ENTRY_SIZE;

   int buffer_size = (page_size-sizeof(struct request_hdr)-sizeof(struct nlmsghdr))-100;
   int entries_per_message =  (buffer_size-2)/ entry_size;

    int i;
   __u8 kernel_buffer[buffer_size];
   __u8 *kernel_buffer_pointer=kernel_buffer;

   error = send_multipart_request_buffer((__u8*)&bib_entries_num,2,SEC_BIB) ;
   if(error)
   {
    	log_err("Something went wrong while sending the bib entries number to the kernel!.");
    	return error;
   }
   __u16 items_sent = 0;
   __u16 items_sent_in_message = 0;

   __u16 real_index = 0;
   while(items_sent < bib_entries_num)
   {
	   kernel_buffer_pointer = kernel_buffer;
	   kernel_buffer_pointer += 2;

	   for(i=0; (i < entries_per_message) && (real_index < bib_entries_num);i++)
	   {
		   kernel_buffer_pointer[0] = bib_buffer[i*entry_size];
		   memcpy(&kernel_buffer_pointer[1],&bib_buffer[i*entry_size+1],4)  ;
		   memcpy(&kernel_buffer_pointer[5],&bib_buffer[i*entry_size+5],2)  ;
		   memcpy(&kernel_buffer_pointer[7],&bib_buffer[i*entry_size+7],16) ;
		   memcpy(&kernel_buffer_pointer[23],&bib_buffer[i*entry_size+23],2);


		   kernel_buffer_pointer+=entry_size;
		   real_index++;
	   }

	   items_sent_in_message = real_index - items_sent;

	   memcpy(kernel_buffer,(__u8*)&items_sent_in_message,2);
	   items_sent = real_index;

	   error = send_multipart_request_buffer(kernel_buffer,buffer_size,SEC_BIB) ;
	   if(error)
	   {
		   log_err("Something went wrong while sending a bib entry to the kernel!.");
		  return error;
	   }
   }


   return error;
}

static int send_multipart_request_buffer(__u8*buffer,
		__u16 request_len, __u16 section)
{

	__u8 buffer_to_send[request_len+2];
	__u8 * section_pointer = (__u8*)&section;

	buffer_to_send[0] = section_pointer[0];
	buffer_to_send[1] = section_pointer[1];

	if(request_len > 0)
	memcpy(&buffer_to_send[2],buffer,request_len);

	return netlink_request_multipart
		(buffer_to_send,request_len+2,MODE_PARSE_FILE,OP_UPDATE) ;
}

#ifdef DEBUG

static int print_config()
{
	int error = 0;
	error = print_global();

	if(error)
	{
		log_info("error while trying to print global section.");
	}

	error = print_pool6();

	if(error)
	{
		log_info("error while trying to print pool6 section.");
	}
	error = print_pool4();

	if(error)
	{
		log_info("error while trying to print pool4 section.");
	}
	error = print_bib();

	if(error)
	{
		log_info("error while trying to print bib section.");
	}


	return 0;

}
static int print_global()
{

		if(configured_parameters->as_fields.manually_enabled) {
			log_info("manually-enabled: %s", (!global->is_disable) ? "true" : "false") ;
		}

		if(configured_parameters->as_fields.address_dependent_filtering)
		{
			log_info("address-dependent-filtering: %s", global->nat64.drop_by_addr ? "true" : "false") ;
		}

		if(configured_parameters->as_fields.drop_icmpv6_info)
		{
			 log_info("drop-icmpv6-info: %s", global->nat64.drop_icmp6_info ? "true" : "false") ;
		}

		if(configured_parameters->as_fields.drop_externally_initiated_tcp)
		{
			log_info("drop-externally-initiated-tcp: %s", global->nat64.drop_external_tcp ? "true" : "false") ;
		}

		if(configured_parameters->as_fields.udp_timeout)
		{
			log_info("udp-timeout: %llu", global->nat64.ttl.udp) ;
		}

		if(configured_parameters->as_fields.tcp_est_timeout) {

			log_info("tcp-est-timeout: %llu", global->nat64.ttl.tcp_est) ;
		}

		if(configured_parameters->as_fields.tcp_trans_timeout)
		{
			log_info("tcp-trans-timeout: %llu", global->nat64.ttl.tcp_trans) ;
		}

		if(configured_parameters->as_fields.icmp_timeout)
		{
			log_info("tcp-trans-timeout: %llu", global->nat64.ttl.tcp_trans) ;
		}

		if(configured_parameters->as_fields.fragment_arrival_timeout)
		{
			log_info("fragment-arrival-timeout: %llu", global->nat64.ttl.frag) ;
		}

		if(configured_parameters->as_fields.maximum_simultaneous_opens)
		{
			log_info("maximum-simultaneous-opens: %llu", global->nat64.max_stored_pkts) ;
		}

		if(configured_parameters->as_fields.source_icmpv6_errors_better)
		{
			log_info("source-icmpv6-errors-better: %s", global->nat64.src_icmp6errs_better ? "true" : "false") ;
		}

		if(configured_parameters->as_fields.logging_bib)
		{
			log_info("logging-bib: %s", global->nat64.bib_logging ? "true" : "false") ;
		}

   		if(configured_parameters->as_fields.logging_session)
  		{
   			log_info("logging-session: %s", global->nat64.bib_logging ? "true" : "false") ;
   		}

		if(configured_parameters->as_fields.zeroize_traffic_class)
		{
			log_info("zeroize-traffic-class: %s", global->reset_traffic_class ? "true" : "false") ;
		}


		if(configured_parameters->as_fields.override_tos)
		{
			log_info("override-tos: %s", global->reset_tos ? "true" : "false") ;
		}

		if(configured_parameters->as_fields.tos)
		{
			log_info("tos: %ul", global->new_tos) ;
		}



		int i;

		log_info("mtu-plateaus-items Number: %u", num_items_mtu_plateaus) ;

		if(configured_parameters->as_fields.mtu_plateaus) {

			for(i=0 ; i < num_items_mtu_plateaus; i++)
			{
				log_info("mtu-plateaus-item #%d: %u",i,global->mtu_plateaus[i]) ;
			}
		}

	return 0;

}
static int print_pool6()
{
	if(pool6_entry) {
		char ipv6_str[32];
		if(!inet_ntop(AF_INET6,&pool6_entry->address,ipv6_str,32))
		{
			log_err("error while trying to print pool6!.");
			return 1;
		}
			log_info("Pool6: %s/%ul", ipv6_str,pool6_entry->len) ;
	}

	return 0;
}
static int print_pool4()
{
	int i = 0;

	log_info("Printing pool4-items...") ;
	log_info("-----------------------") ;
	log_info("pool4-items Ammount: %ul", pool4_entries_num) ;
	log_info("-----------------------") ;
	log_info("") ;


	__u32 mark_value;

	struct ipv4_prefix prefix_value;
	char ipv4_str[16];

	struct port_range port_range_value;
	__u8 is_set_mark;
	__u8 is_set_prefix;
	__u8 is_set_port_range;

	for(i=0; i < pool4_entries_num;i++)
	{
		is_set_mark = pool4_buffer[i*16];
		is_set_port_range = pool4_buffer[(i*16)+5];
		is_set_prefix = pool4_buffer[(i*16)+10];

		log_info("pool4 item #%d:",(i+1));

		if(is_set_mark)
		{
			memcpy((__u8*)&mark_value,&pool4_buffer[(i*16)+1],4);
			log_info("mark: %ul",mark_value);
		}

		if(is_set_prefix)
		{
			memcpy(&prefix_value.address,&pool4_buffer[(i*16)+11],4);
			prefix_value.len = pool4_buffer[(i*16)+15];

			if(!inet_ntop(AF_INET,&(prefix_value.address),ipv4_str,16))
			{
				log_err("error while trying to get Ipv4 address from pool4 item #%d.",(i+1));
				return 1;
			}
			log_info("prefix: %s/%ul",ipv4_str,prefix_value.len);
		}
		else
		{
			log_info("Probably there's something wrong, the flag that indicates the pool4 prefix is set to false!");
		}
		if(is_set_port_range)
		{
			memcpy(&port_range_value.min,&pool4_buffer[(i*16)+6],2);
			memcpy(&port_range_value.max,&pool4_buffer[(i*16)+8],2);
			log_info("port-range: %ul-%ul",port_range_value.min,port_range_value.max);
		}

		log_info("-----------------------") ;
	}

	return 0;

}
static int print_bib()
{
	int i = 0;

	log_info("Printing bib-items...") ;
	log_info("---------------------") ;
	log_info("bib-items Ammount: %ul", bib_entries_num) ;
	log_info("---------------------") ;
	log_info("") ;

	__u8 protocol_value;
	struct ipv4_transport_addr ipv4_addr;
	char ipv4_str[16];
	struct ipv6_transport_addr ipv6_addr;
	char ipv6_str[32];

	for(i = 0; i < bib_entries_num;i++)
	{
		protocol_value = bib_buffer[i*25];

		memcpy(&ipv4_addr.l3,&bib_buffer[i*25+1],4);
		memcpy(&ipv4_addr.l4,&bib_buffer[i*25+5],2);

		if(!inet_ntop(AF_INET,&(ipv4_addr.l3),ipv4_str,16))
		{
			log_err("error while trying to get Ipv4 address from bib item #%d.",(i+1));
			return 1;
		}

		memcpy(&ipv6_addr.l3,&bib_buffer[i*25+7],16);
		memcpy(&ipv6_addr.l4,&bib_buffer[i*25+23],2);

		if(!inet_ntop(AF_INET6,&(ipv6_addr.l3),ipv6_str,32))
		{
			log_err("error while trying to get Ipv6 address from bib item #%d.",(i+1));
			return 1;
		}

		log_info("bib item #%d:",(i+1));
		log_info("protocol: %ui",protocol_value);
		log_info("address_ipv4: %s#%d:",ipv4_str,ipv4_addr.l4);
		log_info("address_ipv6: %s#%d:",ipv6_str,ipv6_addr.l4);
		log_info("---------------------") ;
	}

	return 0;
}

#endif
