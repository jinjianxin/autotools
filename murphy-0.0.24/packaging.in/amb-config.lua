--[[
    Vehicle speed property

    This property has a basic type which is updated often, therefore use
    the built-in handler.
--]]


amb.property {
    name = "vehicle_speed",
    basic_table_name = "amb_vehicle_speed", -- default: "amb_" + name
    dbus_data = {
        obj = "/org/automotive/runningstatus/vehicleSpeed",
        interface = "org.automotive.vehicleSpeed",
        property = "VehicleSpeed",
        notification = "VehicleSpeedChanged",
        signature = "i",
    },
}


--[[
    Shift position property

    Use custom handler for setting the stick status.
--]]

stick_handler = function (self, property)
    -- insert or update the incoming data

    print("shift position handler: " .. property)

    table = self.outputs.test_table

    table:replace({ id = 0, shift_position = property })
end

amb.property {
    name = "stick_handler",
    handler = stick_handler,
    outputs = {
        test_table = mdb.table {
            name = "amb_shift_position",
            index = { "id" },
            create = true,
            columns = {
                { "id", mdb.unsigned },
                { "shift_position", mdb.unsigned },
            }
        }
    },
    dbus_data = {
        obj = "/org/automotive/runningstatus/transmission",
        interface = "org.automotive.transmission",
        property = "ShiftPosition",
        notification = "ShiftPositionChanged",
        signature = "y",
    },
}
