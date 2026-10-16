# main calls app_aws_iot_init(); app_logic is where behaviour slots land, and a
# behaviour that publishes to AWS includes app_aws_iot.h from one of them.
# The JSON pair rides with the framework (see idf_component.yml): a behaviour
# that builds or reads a payload includes json_generator.h / json_parser.h from
# app_logic or main without declaring anything. ONE line per variable — the
# engine reads each line as a map key.
list(APPEND main_PRIV_REQUIRES app_aws_iot espressif__json_parser espressif__json_generator)
list(APPEND app_logic_PRIV_REQUIRES app_aws_iot espressif__json_parser espressif__json_generator)
