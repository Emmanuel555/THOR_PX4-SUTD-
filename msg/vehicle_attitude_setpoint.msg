<<<<<<< HEAD
uint64 timestamp		# time since system start (microseconds)

float32 roll_body		# body angle in NED frame (can be NaN for FW)
float32 pitch_body		# body angle in NED frame (can be NaN for FW)
float32 yaw_body		# body angle in NED frame (can be NaN for FW)
=======
int8 LANDING_GEAR_UP = 1	# landing gear up
int8 LANDING_GEAR_DOWN = -1	# landing gear down

float32 roll_body				# body angle in NED frame
float32 pitch_body				# body angle in NED frame
float32 yaw_body				# body angle in NED frame
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9

float32 yaw_sp_move_rate		# rad/s (commanded by user)

# For quaternion-based attitude control
float32[4] q_d					# Desired quaternion for quaternion control
bool q_d_valid					# Set to true if quaternion vector is valid

<<<<<<< HEAD
# For clarification: For multicopters thrust_body[0] and thrust[1] are usually 0 and thrust[2] is the negative throttle demand.
# For fixed wings thrust_x is the throttle demand and thrust_y, thrust_z will usually be zero.
float32[3] thrust_body		# Normalized thrust command in body NED frame [-1,1]
=======
float32 thrust					# Thrust in Newton the power system should generate
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9

bool roll_reset_integral			# Reset roll integral part (navigation logic change)
bool pitch_reset_integral			# Reset pitch integral part (navigation logic change)
bool yaw_reset_integral				# Reset yaw integral part (navigation logic change)

bool fw_control_yaw					# control heading with rudder (used for auto takeoff on runway)
bool disable_mc_yaw_control			# control yaw for mc (used for vtol weather-vane mode)

uint8 apply_flaps       # flap config specifier
uint8 FLAPS_OFF = 0     # no flaps
uint8 FLAPS_LAND = 1    # landing config flaps
uint8 FLAPS_TAKEOFF = 2 # take-off config flaps

# TOPICS vehicle_attitude_setpoint mc_virtual_attitude_setpoint fw_virtual_attitude_setpoint
