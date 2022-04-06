//uplink payload formatter

function Decoder(bytes, port) {
    // Decode an uplink message from a buffer
    //const header = bytes[0];
    const voltage = bytes[1] / 10.0;
    const battery_pct = bytes[2];
    const temperature = ((bytes[3] << 8) | bytes[4]) / 10.0;
    //const humidity = bytes[5] / 2;
    //const illuminance = ((bytes[6] << 8) | bytes[7]);
    //const pressure = ((bytes[8] << 8) | bytes[9]) * 2.0;
    const co2 = (bytes[10] << 8) | bytes[11];

    // (array) of bytes to an object of fields.
    return {
      //header,
      voltage,
      battery_pct,
      temperature,
      //humidity,
      //illuminance,
      //pressure,
      co2
    };
  }