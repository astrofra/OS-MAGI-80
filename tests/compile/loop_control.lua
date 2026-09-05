-- Multiple continue/break sites exercise the bounded binary CFG funnels.
function loop_control(a: i32, b: i32, c: i32): i32
  local i: i32 = 0
  local total: i32 = a
  while i < 8 do
    i = i + 1
    if i == 2 then
      continue
    else
      total = total + i
    end
    if i == b then
      break
    else
      total = total + c
    end
    if i == 5 then
      continue
    else
      total = total + 1
    end
    if total > 100 then
      break
    else
      total = total
    end
  end
  return total + i
end
