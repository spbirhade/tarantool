require("fiber")
require("box")

local error, print, pairs, type = error, print, pairs, type
local table, tbuf, box, fiber = table, tbuf, box, fiber

module(...)

local def = box.defproc

def("get_first_tupleX3",
    function (txn, namespace, key)
            local index = namespace.index[0]
            local tuple = index[key]
            if tuple then
                    return {tuple, tuple, tuple}
            end
            return nil
    end)


def("get_all_tuples",
    function (txn, namespace)
            local result = {}
            local function next_chunk(index, start)
                    local count, batch_size = 0, 3000
                    for tuple in box.index.treeiter(index, start) do
                            if (count == batch_size) then
                                    -- return last tuple not putting it into result
                                    -- outer iteration will use is as a restart point
                                    return tuple
                            end
                            table.insert(result, tuple)
                            count = count + 1
                    end
                    return nil
            end

            -- iterate over all chunks
            for restart in next_chunk, namespace.index[1] do
                    fiber.sleep(0.001)
            end

            local retcode = 0
            return retcode, result
    end)

box.user_proc['get_all_pkeys'] =
        function (txn, namespace, batch_size)
                local key_count = 0
                if batch_size == nil or batch_size <= 42 then
                        batch_size = 42
                end
                local nspace = tbuf.alloc_fixed(5) -- sizeof(u32) + trailing '\0'
                tbuf.add_iov(nspace, 4)

                local function next_chunk(index, start)
                        local count, b = 0, tbuf.alloc()
                        for tuple in box.index.treeiter(index, start) do
                                if (count == batch_size) then
                                        return tuple, b, count
                                end

                                tbuf.append(b, "f", tuple[0])
                                count = count + 1
                        end
                        -- the loop is done, indicate that returning false and the residual
                        return false, b, count
                end

                -- note, this iterator signals end of iteration by returning false, not nil
                -- this is done because otherwise the residual from last iteration is lost
                for restart, iob, chunk_count in next_chunk, namespace.index[1] do
                        if #iob > 0 then
                                tbuf.add_iov(iob, #iob)
                                key_count = key_count + chunk_count
                        end
                        if restart == false then
                                break
                        end
                        fiber.sleep(0.001)
                end

                tbuf.append(nspace, "u", key_count)

                local retcode = 0
                return retcode
        end
