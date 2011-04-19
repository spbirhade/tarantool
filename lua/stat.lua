stat = {}
stat.records = {}

function stat.print(out)
        local sum = {}
        for i = 1, 5 do
                if type(stat.records[i]) == "table" then
                        for k, v in pairs(stat.records[i]) do
                                sum[k] = (sum[k] or 0) + v
                        end
                end
        end

        tbuf.append(out, "statistics:\r\n")
        for k, v in pairs(sum) do
                v = v / 5
                tbuf.append(out, string.format("  %s: %i\r\n", k, v))
        end
end

function stat.record(rec)
        for i = 5, 1, -1 do
                stat.records[i] = stat.records[i - 1]
        end
        stat.records[0] = rec
end
