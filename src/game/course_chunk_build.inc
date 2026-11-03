f32 Course_SegmentGetJoinLengths(CourseSegment* segment) {
    f32 temp_fv1;
    f32 joinLength;
    f32 length;
    Vec3f sp58;

    segment->joinScale = -1.0f;
    joinLength = gTrackJoinUpperLength[TRACK_SHAPE_INDEX((u32) segment->trackSegmentInfo & TRACK_SHAPE_MASK)];

#ifdef EXPANSION_KIT
    switch (segment->trackSegmentInfo & TRACK_JOIN_MASK) {
        case TRACK_JOIN_PREVIOUS:
        case TRACK_JOIN_NEXT:
            if (segment->length <= (joinLength + 100.0f)) {
                joinLength = segment->length * 0.5f;
            }
            break;
        case TRACK_JOIN_BOTH:
            if (segment->length <= ((2.0f * joinLength) + 100.0f)) {
                joinLength = segment->length * 0.3f;
            }
            break;
    }
#endif

    if (segment->trackSegmentInfo & TRACK_JOIN_PREVIOUS) {
        segment->previousJoinEndTValue = 0.0f;
        length = 0.0f;
        do {
            temp_fv1 = 50.0f / Course_SplineGetTangent(segment, segment->previousJoinEndTValue, &sp58);
            segment->previousJoinEndTValue += temp_fv1;

            if (segment->previousJoinEndTValue >= 1.0f) {
                joinLength = length;
                segment->previousJoinEndTValue -= temp_fv1;
                break;
            }
            length += 50.0f;
        } while (length < joinLength);
        segment->joinScale = length / segment->length;
    } else {
        segment->previousJoinEndTValue = -99.0f;
    }

    if (segment->trackSegmentInfo & TRACK_JOIN_NEXT) {
        segment->nextJoinStartTValue = 1.0f;

        length = 0.0f;
        do {
            temp_fv1 = 50.0f / Course_SplineGetTangent(segment, segment->nextJoinStartTValue, &sp58);
            segment->nextJoinStartTValue -= temp_fv1;
            if (segment->nextJoinStartTValue <= 0.0f) {
                joinLength = length;
                segment->nextJoinStartTValue += temp_fv1;
                break;
            }
            length += 50.0f;
        } while (length < joinLength);
        segment->joinScale = length / segment->length;
    } else if (!(segment->trackSegmentInfo & (TRACK_SHAPE_MASK | TRACK_TYPE_MASK)) &&
               (segment->next->trackSegmentInfo & (TRACK_SHAPE_MASK | TRACK_TYPE_MASK))) {
        segment->nextJoinStartTValue = 1.0f - (100.0f / Course_SplineGetTangent(segment, 1.0f, &sp58));
    } else {
        segment->nextJoinStartTValue = 99.0f;
    }
    if (segment->nextJoinStartTValue <= segment->previousJoinEndTValue) {
        segment->previousJoinEndTValue = 0.49f;
        segment->nextJoinStartTValue = 0.51f;
        segment->joinScale = 0.49f;
        joinLength = segment->length * 0.49f;
    }
    return joinLength;
}

bool func_800A1818(Vec3f* arg0, Vec3f* arg1, Vec3f* arg2) {
    Vec3f vec;
    f32 temp_fa1;

    vec.x = arg2->x - arg0->x;
    vec.y = arg2->y - arg0->y;
    vec.z = arg2->z - arg0->z;
    temp_fa1 = DOT_XYZ(arg1, &vec);
    if (SQ_SUM(&vec) - SQ(temp_fa1) < D_800F89E8) {
        return false;
    }
    return true;
}

f32 Course_ChunkPositionDistance(Vec3f* vec1, Vec3f* vec2) {
    f32 diff;
    f32 sum;

    diff = vec1->x - vec2->x;
    sum = SQ(diff);
    diff = vec1->y - vec2->y;
    sum += SQ(diff);
    diff = vec1->z - vec2->z;
    sum += SQ(diff);

    return sqrtf(sum);
}

s32 func_800A18FC(s32 trackSegmentInfo, f32 distance) {

    if ((trackSegmentInfo & TRACK_TYPE_MASK) != TRACK_TYPE_NONE) {
        return (s32) (TRACK_TYPE_ROW(TRACK_SHAPE_INDEX(trackSegmentInfo & TRACK_SHAPE_MASK),
                                     trackSegmentInfo & TRACK_TYPE_MASK)
                                        ->textureScale *
                      distance) -
               0x8000;
    }
    return -0x8000;
}

extern SegmentChunk gSegmentChunks[];
extern Mtx3F D_80033840[];

s32 func_800A1954(CourseInfo* courseInfo) {
    CourseSegment* startSegment;
    CourseSegment* segment;
    CourseSegment* nextSegment;
    s32 trackSegmentInfo;
    f32 lengthProportionAlongSegment;
    f32 forwardMagnitude;
    f32 radiusLeft;
    f32 radiusRight;
    f32 sp274;
    SegmentChunk* segmentChunk;
    SegmentChunk* nextSegmentChunk;
    f32 t;
    f32 var_fs3;
    f32 var_fs0;
    Vec3f segmentStartLeftPos;
    Vec3f segmentStartRightPos;
    Vec3f segmentStartTopPos;
    Vec3f segmentStartBottomPos;
    Vec3f segmentStartCenterPos;
    Vec3f currentChunkLeftPos;
    Vec3f currentChunkRightPos;
    Vec3f currentChunkTopPos;
    Vec3f currentChunkBottomPos;
    Vec3f currentChunkCenterPos;
    Vec3f previousChunkLeftPos;
    Vec3f previousChunkRightPos;
    Vec3f previousChunkTopPos;
    Vec3f previousChunkBottomPos;
    Vec3f courseStartLeftPos;
    Vec3f courseStartRightPos;
    Vec3f courseStartTopPos;
    Vec3f courseStartBottomPos;
    Vec3f forward;
    Vec3f sp170;
    SegmentChunk sp110;
    Mtx3F segmentBasis;
    f32 leftFaceDistance;
    f32 rightFaceDistance;
    f32 topFaceDistance;
    f32 bottomFaceDistance;
    Vec3f* segmentPos;
    s32 leftTextureCoord;
    s32 rightTextureCoord;
    s32 topTextureCoord;
    s32 bottomTextureCoord;
    s32 var_v1;
    f32 temp_fv0_5;
    unk_800CF528* temp_a3;
    f32 joinLengths;
    s32 spB4;
#ifdef EXPANSION_KIT
    Mtx3F* spB0;
#endif
    Vec3f spA8;
    s32 pad;

    spB4 = 0;
    segment = courseInfo->courseSegments;

    while (segment->trackSegmentInfo & TRACK_FLAG_CONTINUOUS) {
        segment = segment->next;
    }

    startSegment = segment;
    nextSegment = segment->next;
    gSegmentChunks[0].trackSegmentInfo = (segment->trackSegmentInfo & ~TRACK_JOIN_MASK) | TRACK_FLAG_80000000;
    if (segment->trackSegmentInfo & TRACK_JOIN_PREVIOUS) {
        gSegmentChunks[0].trackSegmentInfo |= (TRACK_JOIN_PREVIOUS | TRACK_CHUNK_JOIN_PREVIOUS_START);
    } else if (segment->prev->trackSegmentInfo & TRACK_JOIN_NEXT) {
        gSegmentChunks[0].trackSegmentInfo |= TRACK_JOIN_NEXT;
    }
    joinLengths = Course_SegmentGetJoinLengths(segment);
    forwardMagnitude = Course_SplineGetBasis(segment, 0.0f, &segmentBasis, 0.0f);

#ifdef EXPANSION_KIT
    if (gInCourseEditor) {
        D_80033840[0] = segmentBasis;
        spB0 = &D_80033840[1];
    }
#endif

    Course_SplineGetPosition(segment, 0.0f, &gSegmentChunks[0].pos);
    if (gSegmentChunks[0].trackSegmentInfo & TRACK_JOIN_BOTH) {
        Course_ChunkCalculateRoadAirReferencePos(&gSegmentChunks[0], segment->radiusLeft, segment->radiusRight,
                                                 &segmentBasis, &segmentStartLeftPos, &segmentStartRightPos,
                                                 &segmentStartTopPos, &segmentStartBottomPos, &segmentStartCenterPos);
    } else {
        sSegmentChunkCalculateReferencePosFuncs[TRACK_SHAPE_INDEX(
            (u32) (segment->trackSegmentInfo & TRACK_SHAPE_MASK))](
            &gSegmentChunks[0], segment->radiusLeft, segment->radiusRight, &segmentBasis, &segmentStartLeftPos,
            &segmentStartRightPos, &segmentStartTopPos, &segmentStartBottomPos, &segmentStartCenterPos);
    }
    forward.x = segmentBasis.x.x;
    forward.y = segmentBasis.x.y;
    forward.z = segmentBasis.x.z;

    spA8 = segmentStartLeftPos;
    previousChunkLeftPos = spA8;
    courseStartLeftPos = spA8;
    spA8 = segmentStartRightPos;
    previousChunkRightPos = spA8;
    courseStartRightPos = spA8;
    spA8 = segmentStartTopPos;
    previousChunkTopPos = spA8;
    courseStartTopPos = spA8;
    spA8 = segmentStartBottomPos;
    previousChunkBottomPos = spA8;
    courseStartBottomPos = spA8;

    gSegmentChunks[0].leftTextureCorrection = gSegmentChunks[0].rightTextureCorrection =
        gSegmentChunks[0].topTextureCorrection = gSegmentChunks[0].bottomTextureCorrection = -0x8000;
    gSegmentChunks[0].segmentIndex = (s32) (segment - courseInfo->courseSegments);

    leftFaceDistance = rightFaceDistance = topFaceDistance = bottomFaceDistance = 0.0f;
    var_fs3 = 0.0f;
    t = 0.0f;
    segmentChunk = gSegmentChunks;
    nextSegmentChunk = segmentChunk + 1;
    gSegmentChunks[0].segmentTValue = 0.0f;
    segment->startChunk = gSegmentChunks;
    gSegmentChunkCount = 1;

    while (true) {
        trackSegmentInfo = segment->trackSegmentInfo;
        sp274 = D_800F89E0 / forwardMagnitude;
        if (D_800F8524 != 0) {
            var_fs3 += D_800F89E0;
        }

        if (((segment->trackSegmentInfo & TRACK_SHAPE_MASK) == TRACK_SHAPE_AIR) ||
            (((t == segment->nextJoinStartTValue)) &&
             ((segment->trackSegmentInfo & (TRACK_SHAPE_MASK | TRACK_TYPE_MASK)) ==
              (TRACK_SHAPE_ROAD | ROAD_START_LINE)))) {
            segment->endChunk = nextSegmentChunk;
            segment = nextSegment;
            t = 0.0f;
            if (nextSegment != startSegment) {
                nextSegment = nextSegment->next;
                segment->startChunk = nextSegmentChunk;
                joinLengths = Course_SegmentGetJoinLengths(segment);
            } else {
                break;
            }
        } else if (t < segment->previousJoinEndTValue) {
            t += sp274;

            if (segment->previousJoinEndTValue <= t) {
                t = segment->previousJoinEndTValue;
            }
        } else if ((t < segment->nextJoinStartTValue) && (segment->nextJoinStartTValue < 1.0f)) {
            t += sp274;
            if (segment->nextJoinStartTValue <= t) {
                t = segment->nextJoinStartTValue;
            }
#ifndef EXPANSION_KIT
            // FAKE
            gSegmentChunks[0].trackSegmentInfo = gSegmentChunks[0].trackSegmentInfo;
#endif
        } else {
            t += sp274;

            if (t >= 1.0f) {
                if (nextSegment->trackSegmentInfo & TRACK_FLAG_CONTINUOUS) {
                    if (t < 2.0f) {
                        t -= 1.0f;
                        t *= (forwardMagnitude / Course_SplineGetTangent(nextSegment, 0.0f, &segmentBasis.x));
                    } else {
                        t = 0.0f;
                    }
                } else {
                    t = 0.0f;
                }
                segment->endChunk = nextSegmentChunk;
                segment = nextSegment;
                if (nextSegment != startSegment) {
                    nextSegment = nextSegment->next;
                    if (t == 0.0f) {
                        segment->startChunk = nextSegmentChunk;
                    } else {
                        segment->startChunk = nextSegmentChunk - 1;
                    }
                    joinLengths = Course_SegmentGetJoinLengths(segment);

                } else {
                    break;
                }
            }
        }

        segmentPos = &nextSegmentChunk->pos;
        lengthProportionAlongSegment = Course_SplineGetLengthInfo(segment, t, &sp274);
        radiusLeft =
            ((nextSegment->radiusLeft - segment->radiusLeft) * lengthProportionAlongSegment) + segment->radiusLeft;
        radiusRight =
            ((nextSegment->radiusRight - segment->radiusRight) * lengthProportionAlongSegment) + segment->radiusRight;
        forwardMagnitude = Course_SplineGetBasis(segment, t, &segmentBasis, lengthProportionAlongSegment);
        Course_SplineGetPosition(segment, t, segmentPos);

        sSegmentChunkCalculateReferencePosFuncs[TRACK_SHAPE_INDEX((u32) segment->trackSegmentInfo & TRACK_SHAPE_MASK)](
            nextSegmentChunk, radiusLeft, radiusRight, &segmentBasis, &currentChunkLeftPos, &currentChunkRightPos,
            &currentChunkTopPos, &currentChunkBottomPos, &currentChunkCenterPos);
        leftFaceDistance += Course_ChunkPositionDistance(&currentChunkLeftPos, &previousChunkLeftPos);
        rightFaceDistance += Course_ChunkPositionDistance(&currentChunkRightPos, &previousChunkRightPos);
        topFaceDistance += Course_ChunkPositionDistance(&currentChunkTopPos, &previousChunkTopPos);
        bottomFaceDistance += Course_ChunkPositionDistance(&currentChunkBottomPos, &previousChunkBottomPos);
        previousChunkLeftPos = currentChunkLeftPos;
        previousChunkRightPos = currentChunkRightPos;
        previousChunkTopPos = currentChunkTopPos;
        previousChunkBottomPos = currentChunkBottomPos;
        if ((D_800F8524 == 0) || (D_800F89E4 <= var_fs3) ||
            (func_800A1818(&segmentStartLeftPos, &forward, &currentChunkLeftPos) != 0) ||
            (func_800A1818(&segmentStartRightPos, &forward, &currentChunkRightPos) != 0) ||
            (func_800A1818(&segmentStartTopPos, &forward, &currentChunkTopPos) != 0) ||
            (func_800A1818(&segmentStartBottomPos, &forward, &currentChunkBottomPos) != 0) || (t == 0.0f) ||
            (t == segment->previousJoinEndTValue) || (t == segment->nextJoinStartTValue)) {

            forward.x = currentChunkCenterPos.x - segmentStartCenterPos.x;
            forward.y = currentChunkCenterPos.y - segmentStartCenterPos.y;
            forward.z = currentChunkCenterPos.z - segmentStartCenterPos.z;
            if (0) {}

            if (((sp170.x = currentChunkLeftPos.x - segmentStartLeftPos.x) * forward.x) +
                    ((sp170.y = currentChunkLeftPos.y - segmentStartLeftPos.y) * forward.y) +
                    ((sp170.z = currentChunkLeftPos.z - segmentStartLeftPos.z) * forward.z) >
                0.0f) {
                var_fs0 = sqrtf(SQ(sp170.x) + SQ(sp170.y) + SQ(sp170.z));

                if (((sp170.x = currentChunkRightPos.x - segmentStartRightPos.x) * forward.x) +
                        ((sp170.y = currentChunkRightPos.y - segmentStartRightPos.y) * forward.y) +
                        ((sp170.z = currentChunkRightPos.z - segmentStartRightPos.z) * forward.z) >
                    0.0f) {
                    temp_fv0_5 = sqrtf(SQ(sp170.x) + SQ(sp170.y) + SQ(sp170.z));

                    if (var_fs0 < temp_fv0_5) {
                        sp274 = var_fs0 / temp_fv0_5;
                    } else {
                        sp274 = temp_fv0_5 / var_fs0;
                    }

                    if (sp274 > 0.044f) {
                        if (((sp170.x = currentChunkTopPos.x - segmentStartTopPos.x) * forward.x) +
                                ((sp170.y = currentChunkTopPos.y - segmentStartTopPos.y) * forward.y) +
                                ((sp170.z = currentChunkTopPos.z - segmentStartTopPos.z) * forward.z) >
                            0.0f) {
                            var_fs0 = sqrtf(SQ(sp170.x) + SQ(sp170.y) + SQ(sp170.z));
                            if (((sp170.x = currentChunkBottomPos.x - segmentStartBottomPos.x) * forward.x) +
                                    ((sp170.y = currentChunkBottomPos.y - segmentStartBottomPos.y) * forward.y) +
                                    ((sp170.z = currentChunkBottomPos.z - segmentStartBottomPos.z) * forward.z) >
                                0.0f) {
                                temp_fv0_5 = sqrtf(SQ(sp170.x) + SQ(sp170.y) + SQ(sp170.z));
                                if (var_fs0 < temp_fv0_5) {
                                    sp274 = var_fs0 / temp_fv0_5;
                                } else {
                                    sp274 = temp_fv0_5 / var_fs0;
                                }
                                if (sp274 > 0.034f) {
                                    goto skip;
                                }
                            }
                        }
                    }
                }
            }

            if (!(spB4 & 0x10000)) {
#ifndef EXPANSION_KIT
                spB4 |= 0x10000 | nextSegmentChunk - gSegmentChunks;
#else
                spB4 |= 0x10000;
                if (t < 0.5f) {
                    spB4 |= nextSegmentChunk - gSegmentChunks;
                } else {
                    spB4 |= (nextSegmentChunk - gSegmentChunks) + 1;
                }
#endif
            }

        skip:

            sp274 = SQ(forward.x) + SQ(forward.y) + SQ(forward.z);

#ifdef EXPANSION_KIT
            if (sp274 != 0.0f) {
#endif
                sp274 = 1.0f / sqrtf(sp274);
                forward.x *= sp274;
                forward.y *= sp274;
                forward.z *= sp274;
#ifdef EXPANSION_KIT
            } else {
                forward.x = forward.y = forward.z = 0.0f;
            }
#endif

            segmentStartCenterPos = currentChunkCenterPos;
            segmentStartLeftPos = currentChunkLeftPos;
            segmentStartRightPos = currentChunkRightPos;
            segmentStartTopPos = currentChunkTopPos;
            segmentStartBottomPos = currentChunkBottomPos;

            leftTextureCoord = func_800A18FC(trackSegmentInfo, leftFaceDistance);
            rightTextureCoord = func_800A18FC(trackSegmentInfo, rightFaceDistance);
            topTextureCoord = func_800A18FC(trackSegmentInfo, topFaceDistance);
            bottomTextureCoord = func_800A18FC(trackSegmentInfo, bottomFaceDistance);
            if ((leftTextureCoord < 0x8000) && (rightTextureCoord < 0x8000) && (topTextureCoord < 0x8000) &&
                (bottomTextureCoord < 0x8000)) {
                nextSegmentChunk->leftTextureCoord = leftTextureCoord;
                nextSegmentChunk->rightTextureCoord = rightTextureCoord;
                nextSegmentChunk->topTextureCoord = topTextureCoord;
                nextSegmentChunk->bottomTextureCoord = bottomTextureCoord;
            } else {
                segmentChunk->trackSegmentInfo =
                    (segmentChunk->trackSegmentInfo & ~TRACK_FLAG_CONTINUOUS) | TRACK_FLAG_80000000;
                temp_a3 = TRACK_TYPE_ROW(TRACK_SHAPE_INDEX((u32) trackSegmentInfo & TRACK_SHAPE_MASK),
                                         trackSegmentInfo & TRACK_TYPE_MASK);

                var_v1 = temp_a3->textureCoordinateMask;
                temp_fv0_5 = temp_a3->textureScale;
                segmentChunk->leftTextureCorrection = (segmentChunk->leftTextureCoord & var_v1) - 0x8000;
                nextSegmentChunk->leftTextureCoord =
                    (leftTextureCoord - segmentChunk->leftTextureCoord) + segmentChunk->leftTextureCorrection;
                leftFaceDistance = (f32) (nextSegmentChunk->leftTextureCoord + 0x8000) / temp_fv0_5;

                segmentChunk->rightTextureCorrection = (segmentChunk->rightTextureCoord & var_v1) - 0x8000;
                nextSegmentChunk->rightTextureCoord =
                    (rightTextureCoord - segmentChunk->rightTextureCoord) + segmentChunk->rightTextureCorrection;
                rightFaceDistance = (f32) (nextSegmentChunk->rightTextureCoord + 0x8000) / temp_fv0_5;

                segmentChunk->topTextureCorrection = (segmentChunk->topTextureCoord & var_v1) - 0x8000;
                nextSegmentChunk->topTextureCoord =
                    (topTextureCoord - segmentChunk->topTextureCoord) + segmentChunk->topTextureCorrection;
                topFaceDistance = (f32) (nextSegmentChunk->topTextureCoord + 0x8000) / temp_fv0_5;

                segmentChunk->bottomTextureCorrection = (segmentChunk->bottomTextureCoord & var_v1) - 0x8000;
                nextSegmentChunk->bottomTextureCoord =
                    (bottomTextureCoord - segmentChunk->bottomTextureCoord) + segmentChunk->bottomTextureCorrection;
                bottomFaceDistance = (f32) (nextSegmentChunk->bottomTextureCoord + 0x8000) / temp_fv0_5;
            }
            nextSegmentChunk->trackSegmentInfo =
                segment->trackSegmentInfo &
                ~(TRACK_JOIN_MASK | TRACK_CHUNK_JOIN_TRANSITION_END_MASK | TRACK_CHUNK_JOIN_TRANSITION_START_MASK);
            nextSegmentChunk->segmentIndex = segment - courseInfo->courseSegments;
            nextSegmentChunk->segmentTValue = t;

            if ((segment->previousJoinEndTValue <= t) && (t <= segment->nextJoinStartTValue)) {
                if (t == segment->previousJoinEndTValue) {

                    if ((nextSegmentChunk - 1)->trackSegmentInfo & TRACK_FLAG_INSIDE) {
                        (nextSegmentChunk - 1)->trackSegmentInfo =
                            ((nextSegmentChunk - 1)->trackSegmentInfo & ~TRACK_FLAG_CONTINUOUS) |
                            TRACK_CHUNK_JOIN_PREVIOUS_END;
                    } else {
                        (nextSegmentChunk - 1)->trackSegmentInfo |= TRACK_CHUNK_JOIN_PREVIOUS_END;
                    }
                } else if (t == segment->nextJoinStartTValue) {
                    nextSegmentChunk->trackSegmentInfo |= TRACK_CHUNK_JOIN_NEXT_START;
                    if (!(nextSegmentChunk->trackSegmentInfo & (TRACK_SHAPE_MASK | TRACK_TYPE_MASK))) {
                        nextSegmentChunk->trackSegmentInfo =
                            (nextSegmentChunk->trackSegmentInfo & ~TRACK_TYPE_MASK) | 1;
                    }
                }
            } else {
                if (t < segment->previousJoinEndTValue) {
                    nextSegmentChunk->trackSegmentInfo |= TRACK_JOIN_PREVIOUS;
#ifdef EXPANSION_KIT
                    if (joinLengths != 0.0f) {
#endif
                        sp274 = (segment->length * lengthProportionAlongSegment) / joinLengths;
#ifdef EXPANSION_KIT
                    } else {
                        sp274 = 0.0f;
                    }
#endif
                } else {
                    nextSegmentChunk->trackSegmentInfo |= TRACK_JOIN_NEXT;
                    sp274 = (segment->length - (segment->length * lengthProportionAlongSegment)) / joinLengths;
                }
                sp110.pos = *segmentPos;

                Course_ChunkCalculateRoadAirReferencePos(
                    &sp110, radiusLeft, radiusRight, &segmentBasis, &currentChunkLeftPos, &currentChunkRightPos,
                    &currentChunkTopPos, &currentChunkBottomPos, &currentChunkCenterPos);
                sSegmentChunkJoinFuncs[TRACK_SHAPE_INDEX((u32) (segment->trackSegmentInfo & TRACK_SHAPE_MASK))](
                    &sp110, nextSegmentChunk, sp274);
            }

            if ((t != 0.0f) && (t != segment->previousJoinEndTValue) && (t != segment->nextJoinStartTValue) &&
                (var_fs3 < D_800F89E4)) {
                nextSegmentChunk->trackSegmentInfo |= TRACK_FLAG_CONTINUOUS;
                var_fs3 = 0.0f;
            } else {
                nextSegmentChunk->trackSegmentInfo &= ~TRACK_FLAG_CONTINUOUS;

                var_fs3 = 0.0f;
                if (t == 0.0f) {
                    if (segment->prev->trackSegmentInfo & TRACK_JOIN_NEXT) {

                        nextSegmentChunk->trackSegmentInfo |= TRACK_JOIN_NEXT;
                        if (((nextSegmentChunk - 1)->trackSegmentInfo & TRACK_SHAPE_MASK) == TRACK_SHAPE_CYLINDER) {
                            (nextSegmentChunk - 1)->trackSegmentInfo =
                                (s32) (((nextSegmentChunk - 1)->trackSegmentInfo & ~TRACK_FLAG_CONTINUOUS) |
                                       TRACK_CHUNK_JOIN_NEXT_FINISH);
                        } else {
                            (nextSegmentChunk - 1)->trackSegmentInfo |= TRACK_CHUNK_JOIN_NEXT_FINISH;
                        }
                    }

                    if (nextSegmentChunk->trackSegmentInfo & TRACK_JOIN_PREVIOUS) {
                        nextSegmentChunk->trackSegmentInfo |= TRACK_CHUNK_JOIN_PREVIOUS_START;
                    }
                    nextSegmentChunk->trackSegmentInfo |= TRACK_FLAG_80000000;
                    nextSegmentChunk->leftTextureCorrection = nextSegmentChunk->rightTextureCorrection =
                        nextSegmentChunk->topTextureCorrection = nextSegmentChunk->bottomTextureCorrection = -0x8000;
                    leftFaceDistance = rightFaceDistance = topFaceDistance = bottomFaceDistance = 0.0f;
                }
            }

#ifndef EXPANSION_KIT
            if (++gSegmentChunkCount == SEGMENT_CHUNK_COUNT) {
                spB4 |= 0x20000;
#ifdef PORT
                goto capacity_failure;
#endif
            } else {
                segmentChunk = nextSegmentChunk;
                nextSegmentChunk++;
            }
#else
            if (gSegmentChunkCount < SEGMENT_CHUNK_COUNT) {
                if (gInCourseEditor) {
                    *spB0++ = segmentBasis;
                }
                gSegmentChunkCount++;
                segmentChunk = nextSegmentChunk;
                nextSegmentChunk++;
            } else {
                spB4 |= 0x20000;
#ifdef PORT
                goto capacity_failure;
#endif
            }
#endif
        }
    }

    leftFaceDistance += Course_ChunkPositionDistance(&courseStartLeftPos, &previousChunkLeftPos);
    rightFaceDistance += Course_ChunkPositionDistance(&courseStartRightPos, &previousChunkRightPos);
    topFaceDistance += Course_ChunkPositionDistance(&courseStartTopPos, &previousChunkTopPos);
    bottomFaceDistance += Course_ChunkPositionDistance(&courseStartBottomPos, &previousChunkBottomPos);
    leftTextureCoord = func_800A18FC(trackSegmentInfo, leftFaceDistance);
    rightTextureCoord = func_800A18FC(trackSegmentInfo, rightFaceDistance);
    topTextureCoord = func_800A18FC(trackSegmentInfo, topFaceDistance);
    bottomTextureCoord = func_800A18FC(trackSegmentInfo, bottomFaceDistance);
    if ((leftTextureCoord < 0x8000) && (rightTextureCoord < 0x8000) && (topTextureCoord < 0x8000) &&
        (bottomTextureCoord < 0x8000)) {
        gSegmentChunks[0].leftTextureCoord = leftTextureCoord;
        gSegmentChunks[0].rightTextureCoord = rightTextureCoord;
        gSegmentChunks[0].topTextureCoord = topTextureCoord;
        gSegmentChunks[0].bottomTextureCoord = bottomTextureCoord;
    } else {
        segmentChunk->trackSegmentInfo =
            (segmentChunk->trackSegmentInfo & ~TRACK_FLAG_CONTINUOUS) | TRACK_FLAG_80000000;
        temp_a3 = TRACK_TYPE_ROW(TRACK_SHAPE_INDEX((u32) trackSegmentInfo & TRACK_SHAPE_MASK),
                                 trackSegmentInfo & TRACK_TYPE_MASK);
        var_v1 = temp_a3->textureCoordinateMask;

        segmentChunk->leftTextureCorrection = (segmentChunk->leftTextureCoord & var_v1) - 0x8000;
        gSegmentChunks[0].leftTextureCoord =
            (leftTextureCoord - segmentChunk->leftTextureCoord) + segmentChunk->leftTextureCorrection;

        segmentChunk->rightTextureCorrection = (segmentChunk->rightTextureCoord & var_v1) - 0x8000;
        gSegmentChunks[0].rightTextureCoord =
            (rightTextureCoord - segmentChunk->rightTextureCoord) + segmentChunk->rightTextureCorrection;

        segmentChunk->topTextureCorrection = (segmentChunk->topTextureCoord & var_v1) - 0x8000;
        gSegmentChunks[0].topTextureCoord =
            (topTextureCoord - segmentChunk->topTextureCoord) + segmentChunk->topTextureCorrection;

        segmentChunk->bottomTextureCorrection = (segmentChunk->bottomTextureCoord & var_v1) - 0x8000;
        gSegmentChunks[0].bottomTextureCoord =
            (bottomTextureCoord - segmentChunk->bottomTextureCoord) + segmentChunk->bottomTextureCorrection;
    }
    if (nextSegment->prev->trackSegmentInfo & TRACK_JOIN_NEXT) {
        if ((segmentChunk->trackSegmentInfo & TRACK_SHAPE_MASK) == TRACK_SHAPE_CYLINDER) {
            segmentChunk->trackSegmentInfo =
                (segmentChunk->trackSegmentInfo & ~TRACK_FLAG_CONTINUOUS) | TRACK_CHUNK_JOIN_NEXT_FINISH;
        } else {
            segmentChunk->trackSegmentInfo |= TRACK_CHUNK_JOIN_NEXT_FINISH;
        }
    }

    sLastSegmentChunk = &gSegmentChunks[gSegmentChunkCount];
    gSegmentChunks[gSegmentChunkCount] = gSegmentChunks[0];
    return spB4;
#ifdef PORT
capacity_failure:
    for (s32 i = 0; i < courseInfo->segmentCount; ++i) {
        courseInfo->courseSegments[i].startChunk = NULL;
        courseInfo->courseSegments[i].endChunk = NULL;
    }
    gSegmentChunkCount = 0;
    sLastSegmentChunk = NULL;
    return spB4;
#endif
}
