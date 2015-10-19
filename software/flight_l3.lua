type = FLIGHT_CONTROL
name = "L3"


function rotate(ship_state, rotation)
    states = {}
    thruster_i = 0

    while ship_state.thrusters[thruster_i] ~= nil do
        thruster = ship_state.thrusters[thruster_i]
        state = 0.0

        if thruster.type == THRUSTER_RCS then
            -- This doesn't work with arbitrary thruster directions. Who cares.
            torque = crossp(thruster.relative_position, thruster.force)
            if torque:length() > 0.0 then
                state = dotp(torque, rotation) / torque:length()
            end
        end

        states[thruster_i] = state

        thruster_i = thruster_i + 1
    end

    return states
end


function rotate_to(ship_state, proj)
    if proj.z < 0.0 then
        target_vector = vec3(proj.y, -proj.x, 0.0)
    elseif proj:length() > 0.0 then
        apx = math.abs(proj.x)
        apy = math.abs(proj.y)

        if apx > apy then
            target_vector = vec3(proj.y / apx, -proj.x / apx, 0.0)
        else
            target_vector = vec3(proj.y / apy, -proj.x / apy, 0.0)
        end
    else
        target_vector = vec3(0.0, 1.0, 0.0)
    end

    rvel = ship_state.rotational_velocity

    rvl = rvel:length()
    if rvl < 0.001 then
        rvl = 0.001
    end

    tvl = target_vector:length()

    stop_way = rvl * ship_state.total_mass / 200.0

    -- The 10.0 in rotate() stands for "UP TO THE MAX"
    if tvl / rvl <= stop_way then
        return rotate(ship_state, -10.0 * rvel)
    else
        return rotate(ship_state, 10.0 * (target_vector - rvel))
    end
end


function flight_control(ship_state, input, interval)
    if interval >= 0.5 then
        -- The above becomes unstable with big intervals
        return nil
    end

    if input.prograde then
        return rotate_to(ship_state,  ship_state.velocity:normalized())
    elseif input.retrograde then
        return rotate_to(ship_state, -ship_state.velocity:normalized())
    elseif input.orbit_normal then
        return rotate_to(ship_state,  ship_state.orbit_normal)
    elseif input.orbit_antinormal then
        return rotate_to(ship_state, -ship_state.orbit_normal)
    end

    return nil
end
