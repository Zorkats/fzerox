        case 0x12:
            if (D_80794E14) {
                break;
            }
#ifdef PORT
            if (gMfsError != LEO_ERROR_GOOD) {
                gdx_course_edit_flash_message("Course not saved. Keep editing or check gdiffuser-run.log before retrying.");
                gCourseEditFileOption = -1;
                func_8070405C(true);
                D_800D6CA0.unk_08 = 0;
                break;
            }
#endif
            switch (D_80119880) {
                case -1:
                case 1:
                    if (COURSE_CONTEXT()->courseData.flag == 0) {
                        func_xk2_800EBFE8(D_xk1_8003A598.name);
                        D_800D6CA0.unk_08 = 0x22;
                    } else {
                        D_800D6CA0.unk_08 = 0;
                    }
                    break;
                case 9:
                    if (D_xk2_800F7408.courseData.flag == 0) {
                        func_xk2_800EBFE8(D_xk1_8003A598.name);
                        D_800D6CA0.unk_08 = 0x22;
                    } else {
                        D_800D6CA0.unk_08 = 0;
                    }
                    break;
            }
            break;
