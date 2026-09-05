-- Exact-width integer arithmetic, normalization, comparison, and division.
function narrow(a: i8, b: u8, c: i16): u16
  local si: i8 = a + 1
  local ub: u8 = b + 2
  local quotient: u8 = b
  quotient /= ub
  local sw: i16 = c / -2
  local out: u16 = 65535
  if si < -1 then
    if quotient > 200 then out = 1 else out = 2 end
  else
    if sw <= -3 then out = 3 else out = 4 end
  end
  return out
end
