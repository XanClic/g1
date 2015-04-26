type = SCENARIO
name = "0"


function initialize()
    fix_player_to_ground(true)

    set_player_position(-1.40673537711, 0.495382619358, 10)
    set_player_bearing(0)
end


total_time = 0.0
fixed_to_ground = true
turn_time0 = 0.0
turn_time1 = 0.0
drop_time = 0.0
turn_time2 = 0.0
acceleration = 0.0

function step(interval, ship_state)
    total_time = total_time + interval

    if total_time < 5.0 then
        return nil
    elseif fixed_to_ground and ship_state.velocity:length() < 486 then -- up to ~150 kn (incl ~409 m/s orbital speed)
        -- accelerate

        ship_state.position = ship_state.position + interval * ship_state.velocity / 1000.0
        ship_state.position = 6371.01 / ship_state.position:length() * ship_state.position
        ship_state.velocity = ship_state.velocity + 3.0 * interval * ship_state.forward
    elseif ship_state.position:length() < 6386 then -- up to 15 km height
        -- launch from ground

        if fixed_to_ground then
            fix_player_to_ground(false)
            fixed_to_ground = false
            acceleration = 3.0
        end

        turn_time0 = turn_time0 + interval

        ship_state.position = ship_state.position + interval * ship_state.velocity / 1000.0

        if acceleration > 0.0 then
            velocity_value = ship_state.velocity:length() + acceleration * interval
            ship_state.velocity = velocity_value * ship_state.forward

            acceleration = acceleration - interval * 4.5 / 145.0 -- 3.0^2 / (2.0 * 145.0)
        end

        if turn_time0 / 4.0 < math.pi then
            ship_state.forward = ship_state.forward:rotate(ship_state.right, interval * math.sin(turn_time0 / 4.0) * 0.13 / 4.0)
            ship_state.up = nil
        end
    elseif turn_time1 / 10.0 < 1 + math.pi then
        -- turn level and wait ten seconds

        turn_time1 = turn_time1 + interval

        ship_state.position = ship_state.position + interval * ship_state.velocity / 1000.0

        if turn_time1 / 10.0 < math.pi then
            ship_state.velocity = ship_state.velocity:length() * ship_state.forward
            ship_state.forward = ship_state.forward:rotate(ship_state.right, interval * -math.sin(turn_time1 / 10.0) * 0.13 / 10.0)
            ship_state.up = nil
        end
    elseif drop_time < 5 then
        -- drop

        drop_time = drop_time + interval

        ship_state.position = ship_state.position + interval * ship_state.velocity / 1000.0
        ship_state.velocity = ship_state.velocity - 9.81 * interval * ship_state.up
    elseif ship_state.position:length() < 6471 then
        -- ignite booster, accelerate

        turn_time2 = turn_time2 + interval

        ship_state.position = ship_state.position + interval * ship_state.velocity / 1000.0
        ship_state.velocity = ship_state.velocity + 30.0 * interval * ship_state.forward

        if turn_time2 / 5.0 < math.pi then
            ship_state.forward = ship_state.forward:rotate(ship_state.right, interval * math.sin(turn_time2 / 5.0) * 0.5 / 5.0)
            ship_state.up = nil
        end
    else
        enable_player_physics(true)
    end

    return ship_state
end
