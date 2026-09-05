-- Loop-carried values, a parallel-copy cycle, and a conditional in the body.
function loops(a: i32, b: i32, c: i32): i32
  local i: i32 = 0
  local x: i32 = a
  local y: i32 = b
  local temp: i32 = 0
  local total: i32 = c
  while i < 5 do
    temp = x
    x = y
    y = temp
    if i != 2 then
      total = total + x + i
    else
      total = total - y
    end
    i = i + 1
  end
  return total + x * 3 - y
end
