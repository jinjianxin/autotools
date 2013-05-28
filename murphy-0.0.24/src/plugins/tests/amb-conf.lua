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
        signature = "i",
    },
}



--[[
    Mirror position property

    This property has a complex type (associative array from mirror id
    to tuple containing the mirror tilt and pan value) and is expected
    to change only rarely, so use a custom handler.
--]]


mdb.table {
    name = "amb_mirrors",
    index = { "mirror_id" },
    create = true,
    columns = {
        { "mirror_id", mdb.unsigned },
        { "tilt", mdb.unsigned },
        { "pan", mdb.unsigned }
    }
}

mirror_handler = function (self, property)
    -- insert or update the incoming data

    for val_mirror_id, values in pairs(property) do
        val_tilt = values[0]
        val_pan = values[1]

        mdb.table.amb_mirrors[val_mirror_id] = {
            mirror_id = val_mirror_id,
            tilt = val_tilt,
            pan = val_pan
        }
    end
end

amb.property {
    name = "mirror_setttings",
    handler = mirror_handler,
    dbus_data = {
        obj = "/org/automotive/runningstatus/personalization",
        interface = "org.automotive.mirrorSettings",
        property = "MirrorSettings",
        signature = "a{y(yy)}",
    },
}



--[[
    Test handlers
--]]


test_handler = function (self, property)
    -- insert or update the incoming data

    print("> test handler")

    print("test handler: " .. property)

    print("name: " .. self.name)

    d = self.dbus_data

    print("assigned to d")

    print("d.obj      : " .. d.obj)
    print("d.interface: " .. d.interface)
    print("d.property : " .. d.property)
    print("d.signature: " .. d.signature)

    table = self.outputs.test_table

    print("table name: " .. table.name)

    table:replace({ test_value = property })

end

amb.property {
    name = "test",
    handler = test_handler,
    outputs = {
        test_table = mdb.table {
            name = "amb_test",
            index = { "test_value" },
            create = true,
            columns = {
                { "test_value", mdb.string, 32 },
            }
        }
    },
    dbus_data = {
        obj = "/org/automotive/test",
        interface = "org.automotive.test",
        property = "Test",
        signature = "s",
    },
}



--[[
    Test handlers for complex types - struct
--]]

test_struct_handler = function (self, property)
    print("test_struct_handler")
    print("got values: " .. property[1] .. " and " .. property[2])
end

amb.property {
    name = "test_struct",
    handler = test_struct_handler,
    outputs = {},
    dbus_data = {
        obj = "/org/automotive/test1",
        interface = "org.automotive.test",
        property = "Test",
        signature = "(su)",
    },
}

--[[
    Test handlers for complex types - array
--]]

test_array_handler = function (self, property)
    print("test_array_handler")
    print("got values: " .. property[1] .. " and " .. property[2])
end

amb.property {
    name = "test_array",
    handler = test_array_handler,
    outputs = {},
    dbus_data = {
        obj = "/org/automotive/test2",
        interface = "org.automotive.test",
        property = "Test",
        signature = "as",
    },
}


--[[
    Test handlers for complex types - dict
--]]

test_dict_handler = function (self, property)
    print("test_dict_handler")
    print("got values:")
    for key, value in pairs(property) do
        print(key .. " -> " .. value)
    end
end

amb.property {
    name = "test_dict",
    handler = test_dict_handler,
    outputs = {},
    dbus_data = {
        obj = "/org/automotive/test3",
        interface = "org.automotive.test",
        property = "Test",
        signature = "a{su}",
    },
}


--[[
    Test handlers for complex types - nested
--]]

test_nested_handler = function (self, property)
    print("test_nested_handler")
    print("got values:")
    for key, value in pairs(property) do
        print(key .. " -> (" .. value[1] .. ", " .. value[2] ..")")
    end
end

amb.property {
    name = "test_nested",
    handler = test_nested_handler,
    outputs = {},
    dbus_data = {
        obj = "/org/automotive/test4",
        interface = "org.automotive.test",
        property = "Test",
        signature = "a{s(uu)}",
    },
}

