m = murphy.get()

-- load the dlog plugin
m:try_load_plugin('dlog')

-- load the console plugin
m:try_load_plugin('console')

m:try_load_plugin('console.disabled', 'webconsole', {
                  address = 'wsck:127.0.0.1:3000/murphy',
                  httpdir = '/usr/share/murphy/webconsole' });

-- load the dbus plugin
if m:plugin_exists('dbus') then
    m:load_plugin('dbus')
end

-- load the native resource plugin
if m:plugin_exists('resource-native') then
    m:load_plugin('resource-native')
    m:info("native resource plugin loaded")
else
    m:info("No native resource plugin found...")
end

-- load the dbus resource plugin
m:try_load_plugin('resource-dbus', {
    dbus_bus = "system",
    dbus_service = "org.Murphy",
    dbus_track = true,
    default_zone = "driver",
    default_class = "implicit"
})

-- load the domain control plugin
if m:plugin_exists('domain-control') then
    m:load_plugin('domain-control')
else
    m:info("No domain-control plugin found...")
end

-- load the AMB plugin
if m:plugin_exists('amb') then
    m:load_plugin('amb')
else
    m:info("No amb plugin found...")
end

-- load the ASM resource plugin
if m:plugin_exists('resource-asm') then
    m:load_plugin('resource-asm', {
        zone = "driver",
        share_mmplayer = "player:AVP,mandatory,exclusive,relaxed"
    })
else
    m:info("No audio session manager plugin found...")
end

-- define application classes
application_class { name="interrupt", priority=99, modal=true , share=false, order="fifo" }
application_class { name="alert"    , priority=51, modal=false, share=false, order="fifo" }
application_class { name="navigator", priority=50, modal=false, share=true , order="fifo" }
application_class { name="phone"    , priority=6 , modal=false, share=true , order="lifo" }
application_class { name="camera"   , priority=5 , modal=false, share=false, order="lifo" }
application_class { name="event"    , priority=4 , modal=false, share=true , order="fifo" }
application_class { name="game"     , priority=3 , modal=false, share=false, order="lifo" }
application_class { name="player"   , priority=1 , modal=false, share=true , order="lifo" }
application_class { name="implicit" , priority=0 , modal=false, share=false, order="lifo" }

-- define zone attributes
zone.attributes {
    type = {mdb.string, "common", "rw"},
    location = {mdb.string, "anywhere", "rw"}
}

-- define zones
zone {
     name = "driver",
     attributes = {
         type = "common",
         location = "front-left"
     }
}

zone {
     name = "passanger1",
     attributes = {
         type = "private",
         location = "front-right"
     }
}

zone {
     name = "passanger2",
     attributes = {
         type = "private",
         location = "back-left"
     }
}

zone {
     name = "passanger3",
     attributes = {
         type = "private",
         location = "back-right"
     }
}

zone {
     name = "passanger4",
     attributes = {
         type = "private",
         location = "back-left"
     }
}


-- define resource classes
resource.class {
     name = "audio_playback",
     shareable = true,
     attributes = {
         role   = { mdb.string, "music"    , "rw" },
         pid    = { mdb.string, "<unknown>", "rw" },
         policy = { mdb.string, "strict"   , "rw" }
     }
}

resource.class {
     name = "audio_recording",
     shareable = true,
     attributes = {
         role   = { mdb.string, "music"    , "rw" },
         pid    = { mdb.string, "<unknown>", "rw" },
         policy = { mdb.string, "relaxed"  , "rw" }
     }
}

resource.class {
     name = "video_playback",
     shareable = false,
}

resource.class {
     name = "video_recording",
     shareable = false
}

resource.method.veto = {
    function(zone, rset, grant, owners)
	rset_priority = application_class[rset.application_class].priority

	owner_id = owners.audio_playback.resource_set
	rset_id = rset.id

        if (rset_priority >= 50 and owner_id ~= rset_id) then
            print("*** resource-set "..rset_id.." - veto")
            return false
        end

        return true
    end
}

-- test for creating selections
mdb.select {
           name = "audio_owner",
           table = "audio_playback_owner",
           columns = {"application_class"},
           condition = "zone_name = 'driver'"
}

mdb.select {
           name = "vehicle_speed",
           table = "amb_vehicle_speed",
           columns = {"value"},
           condition = "key = 'VehicleSpeed'"
}

element.lua {
   name    = "speed2volume",
   inputs  = { speed = mdb.select.vehicle_speed, param = 9 },
   outputs = {  mdb.table { name = "speedvol",
			    index = {"zone", "device"},
			    columns = {{"zone", mdb.string, 16},
				       {"device", mdb.string, 16},
				       {"value", mdb.floating}},
                            create = true
			   }
	     },
   oldvolume = 0.0,
   update  = function(self)
                speed = self.inputs.speed.single_value
                if (speed) then
                    volume = (speed - 144.0) / 7.0
                else
                    volume = 0.0
                end
                diff = volume - self.oldvolume
                if (diff*diff > self.inputs.param) then
                    print("*** element "..self.name.." update "..volume)
                    self.oldvolume = volume
		    mdb.table.speedvol:replace({zone = "driver", device = "speakers", value = volume})
		end
	     end
}

-- load the telephony plugin
m:try_load_plugin('telephony')
