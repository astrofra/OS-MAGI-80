-- Keep several values live to force D3/D4 allocation and MOVEM preservation.
function register_pressure(a: i32, b: i32, c: i32): i32
  return ((a * b) + (a * c)) + ((b * c) + (a * b))
end
