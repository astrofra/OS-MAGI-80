-- Typed locals, reassignment, local-to-local flow, and one dead final store.
function locals(a: i32, b: i32, c: i32): i32
  local x: i32 = a * 3 + b
  local y: i32 = x + c
  x = y - a
  y = x * 2
  x = 12345
  return y + c
end
