#include <Shlwapi.h>
#include "os/system.h"

namespace aten
{
    bool SetCurrentDirectoryFromExe()
    {
        static char buf[_MAX_PATH];

        // ���s�v���O�����̃t���p�X���擾
        {
            DWORD result = ::GetModuleFileName(
                NULL,
                buf,
                sizeof(buf));
            AT_ASSERT(result > 0);
        }

        // �t�@�C��������菜��
        auto result = ::PathRemoveFileSpec(buf);
        AT_ASSERT(result);

        // �J�����g�f�B���N�g����ݒ�
        result = ::SetCurrentDirectory(buf);
        AT_ASSERT(result);

        return result ? true : false;
    }
}
