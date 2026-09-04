-- bool locals, all six i32 comparisons, nested branches, and value joins.
function conditionals(a: i32, b: i32, c: i32): i32
  local result: i32 = a + b + c
  local ascending: bool = a < b
  if ascending == true then
    if b <= c then
      result = result + 10
    else
      result = result + 20
    end
  else
    if a >= c then
      result = result - 30
    else
      result = result - 40
    end
  end
  if a == b then
    result = result + 100
  else
    result = result - 100
  end
  if a ~= c then
    result = result + 1000
  else
    result = result - 1000
  end
  if c > b then
    result = result + 10000
  else
    result = result - 10000
  end
  return result
end
