# esp_matter rewrites its generated *BuildConfig.h headers on every CMake run;
# write them only when their content changes, so CHIP is not recompiled.
set(_zc_matter_gen "${CMAKE_CURRENT_SOURCE_DIR}/managed_components/espressif__esp_matter/managed_component_include")
if(EXISTS "${_zc_matter_gen}")
  execute_process(
    COMMAND python3 -c "
import os, sys
d = sys.argv[1]
key = 'file(WRITE ' + chr(36) + '{CMAKE_CURRENT_BINARY_DIR}/'
stop = ' ' + chr(9) + chr(10) + chr(13) + chr(34) + ')'
done = 0
for name in sorted(os.listdir(d)):
    if not name.endswith('.cmake'):
        continue
    p = os.path.join(d, name)
    with open(p) as f: s = f.read()
    if '.zc-new' in s:
        continue
    out, paths, pos = [], [], 0
    while True:
        i = s.find(key, pos)
        if i < 0:
            break
        j = i + len('file(WRITE ')
        k = i + len(key)
        while k < len(s) and s[k] not in stop:
            k += 1
        path = s[j:k]
        if path.endswith('.h'):
            out.append(s[pos:k] + '.zc-new')
            if path not in paths: paths.append(path)
        else:
            out.append(s[pos:k])
        pos = k
    if not paths:
        continue
    out.append(s[pos:])
    t = ''.join(out)
    for q in paths:
        t += chr(10) + 'file(COPY_FILE ' + q + '.zc-new ' + q + ' ONLY_IF_DIFFERENT)' + chr(10)
    with open(p, 'w') as f: f.write(t)
    done += 1
if done: print('patched', done, 'files')
" "${_zc_matter_gen}"
    OUTPUT_VARIABLE _zc_gen_out
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(_zc_gen_out)
    message(STATUS "esp_matter build-config headers: written only when changed (${_zc_gen_out})")
  endif()
endif()
