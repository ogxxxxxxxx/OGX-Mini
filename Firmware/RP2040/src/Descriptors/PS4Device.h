    // Limpiamos bits de d-pad
    report.buttons &= ~( PS4Dev::Buttons::DPAD_UP
                       | PS4Dev::Buttons::DPAD_DOWN
                       | PS4Dev::Buttons::DPAD_LEFT
                       | PS4Dev::Buttons::DPAD_RIGHT );

    switch (gp_out.dpad) {
        case Gamepad::DPAD_UP:
            report.hat = PS4Dev::Hat::UP;
            report.buttons |= PS4Dev::Buttons::DPAD_UP;
            break;
        case Gamepad::DPAD_DOWN:
            report.hat = PS4Dev::Hat::DOWN;
            report.buttons |= PS4Dev::Buttons::DPAD_DOWN;
            break;
        case Gamepad::DPAD_LEFT:
            report.hat = PS4Dev::Hat::LEFT;
            report.buttons |= PS4Dev::Buttons::DPAD_LEFT;
            break;
        case Gamepad::DPAD_RIGHT:
            report.hat = PS4Dev::Hat::RIGHT;
            report.buttons |= PS4Dev::Buttons::DPAD_RIGHT;
            break;
        case Gamepad::DPAD_UP_RIGHT:
            report.hat = PS4Dev::Hat::UP_RIGHT;
            report.buttons |= PS4Dev::Buttons::DPAD_UP;
            report.buttons |= PS4Dev::Buttons::DPAD_RIGHT;
            break;
        case Gamepad::DPAD_DOWN_RIGHT:
            report.hat = PS4Dev::Hat::DOWN_RIGHT;
            report.buttons |= PS4Dev::Buttons::DPAD_DOWN;
            report.buttons |= PS4Dev::Buttons::DPAD_RIGHT;
            break;
        case Gamepad::DPAD_DOWN_LEFT:
            report.hat = PS4Dev::Hat::DOWN_LEFT;
            report.buttons |= PS4Dev::Buttons::DPAD_DOWN;
            report.buttons |= PS4Dev::Buttons::DPAD_LEFT;
            break;
        case Gamepad::DPAD_UP_LEFT:
            report.hat = PS4Dev::Hat::UP_LEFT;
            report.buttons |= PS4Dev::Buttons::DPAD_UP;
            report.buttons |= PS4Dev::Buttons::DPAD_LEFT;
            break;
        default:
            report.hat = PS4Dev::Hat::CENTER;
            break;
    }
