# main calls app_aws_iot_init(); app_logic is where behaviour slots land, and a
# behaviour that publishes to AWS includes app_aws_iot.h from one of them.
list(APPEND main_PRIV_REQUIRES app_aws_iot)
list(APPEND app_logic_PRIV_REQUIRES app_aws_iot)
