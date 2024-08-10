idf_component_register(
    SRCS "data.cpp" "sensors.cpp" "main.cpp" "hashing/hashing.cpp" "sd/sd.cpp" "sending/sending.cpp" "libs/DS3231/DS3231.cpp" "libs/TCA9548/TCA9548" "libs/veml3328_Arduino/veml3328_Arduino.cpp" "libs/WireUtils/WireUtils.cpp" "libs/veml7700_Arduino/veml7700_Arduino.cpp" "libs/CPP_Utils/src/comps/ByteStreamWriter.cpp"
    INCLUDE_DIRS "libs" "libs/WireUtils"
)
