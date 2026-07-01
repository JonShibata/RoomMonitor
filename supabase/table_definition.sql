-- Create a function to define the standard room monitor table schema
-- This makes it easier to ensure all tables have the same columns
CREATE OR REPLACE FUNCTION create_room_table(table_name text)
RETURNS void AS $$
BEGIN
    EXECUTE format('
        CREATE TABLE IF NOT EXISTS %I (
            id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
            created_at timestamp with time zone DEFAULT now(),
            room_name text,
            "Door" boolean,
            "Temperature" float4,
            "Humidity" float4,
            "Motion" boolean,
            "Light1" integer,
            "Light2" integer,
            "LightAlert" boolean,
            "DoorAlert" boolean,
            "LightAlertTrig" boolean,
            "DoorAlertTrig" boolean,
            "Daylight" boolean
        );
    ', table_name);
END;
$$ LANGUAGE plpgsql;

-- Create tables for each of the rooms defined in the config
SELECT create_room_table('Basement');
SELECT create_room_table('BonusRoom');
SELECT create_room_table('Garage');
SELECT create_room_table('Test');

-- (Optional) Drop the function if no longer needed
-- DROP FUNCTION create_room_table(text);
