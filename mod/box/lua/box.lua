
local error, print, type, pairs, setfenv, getmetatable =
      error, print, type, pairs, setfenv, getmetatable
local tbuf = tbuf

module(...)

user_proc = {}
REPLACE = 13
UPDATE_FIELDS = 19
DELETE = 20


-- TODO: select from multi column indexes
function select(namespace, ...)
        local index = namespace.index[0]
        local result = {}
        for k, v in pairs({...}) do
                result[k] = index[v]
        end
        return result
end

function replace(namespace, ...)
        local tuple = {...}
        local flags = 0
        local txn = txn.alloc()
        local req = tbuf.alloc()
        tbuf.append(req, "uuu", namespace.n, flags, #tuple)
        for k, v in pairs(tuple) do 
                tbuf.append(req, "f", v)
        end
        dispatch(txn, REPLACE, req)
end

function delete(namespace, key)
        local txn = txn.alloc()
        local req = tbuf.alloc()
        local key_len = 1
        tbuf.append(req, "uuf", namespace.n, key_len, key)
        dispatch(txn, DELETE, req)
end


function defproc(name, proc_body, env)
        if type(proc_body) == "string" then
                proc_body = loadstring(code)
        end
        if type(proc_body) ~= "function" then 
                return nil 
        end

        local tbuf, box = tbuf, _M
        local function proc(txn, namespace, ...)
                local retcode, result = proc_body(txn, namespace, ...)

                local nspace = tbuf.alloc_fixed(5) -- sizeof(u32) + trailing '\0'
                tbuf.add_iov(nspace, 4)

                if type(result) == "table" then
                        for k, v in pairs(result) do
                                if type(v) ~= "userdata" then
                                        error("unexpected type of result: " .. type(v))
                                end

                                local metatable = getmetatable(v)
                                if metatable == "Tarantool.box.tuple" then
                                        box.tuple_add_iov(txn, v)
                                elseif metatable == "Tarantool.tbuf" then
                                        tbuf.add_iov(v, #v)
                                else
                                        error("unexpected metatable of result: " .. metatable)
                                end
                        end
                        tbuf.append(nspace, "u", #result)
                elseif type(result) == "number" then
                        tbuf.append(nspace, "u", result)
                else
                        error("unexpected type of result:" .. type(result))
                end

                return retcode
        end

        setfenv(proc, env or {})
        user_proc[name] = proc
end
