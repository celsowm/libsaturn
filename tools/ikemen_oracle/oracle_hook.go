package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"os"
	"strconv"
)

const libsaturnOracleSchema = 1

type libsaturnOracleChar struct {
	PlayerNo        int       `json:"player_no"`
	HelperIndex     int       `json:"helper_index"`
	ID              int32     `json:"id"`
	HelperID        int32     `json:"helper_id"`
	ParentID        int32     `json:"parent_id"`
	TeamSide        int       `json:"team_side"`
	StateNo         int32     `json:"state_no"`
	StateTime       int32     `json:"state_time"`
	StateType       int32     `json:"state_type"`
	MoveType        int32     `json:"move_type"`
	Ctrl            bool      `json:"ctrl"`
	Anim            int32     `json:"anim"`
	AnimElem        int32     `json:"anim_elem"`
	AnimTime        int32     `json:"anim_time"`
	Pos             [3]float32 `json:"pos"`
	Vel             [3]float32 `json:"vel"`
	Facing          float32   `json:"facing"`
	Life            int32     `json:"life"`
	Power           int32     `json:"power"`
	Juggle          int32     `json:"juggle"`
	HitPause        int32     `json:"hit_pause"`
	MoveContactType int32     `json:"move_contact_type"`
	MoveContactTime int32     `json:"move_contact_time"`
	GetHitChainID   int32     `json:"gethit_chain_id"`
	Targets         []int32   `json:"targets"`
	HitDefTargets   []int32   `json:"hitdef_targets"`
}

type libsaturnOracleProjectile struct {
	PlayerNo   int        `json:"player_no"`
	OwnerID    int32      `json:"owner_id"`
	ID         int32      `json:"id"`
	Status     int32      `json:"status"`
	Anim       int32      `json:"anim"`
	AnimElem   int32      `json:"anim_elem"`
	Time       int32      `json:"time"`
	Pos        [3]float32 `json:"pos"`
	Vel        [3]float32 `json:"vel"`
	Facing     float32    `json:"facing"`
	Hits       int32      `json:"hits"`
	MissTime   int32      `json:"miss_time"`
	HitPause   int32      `json:"hit_pause"`
	Contact    bool       `json:"contact"`
	Remove     bool       `json:"remove"`
	RemoveTime int32      `json:"remove_time"`
}

type libsaturnOracleFrame struct {
	Schema      int                          `json:"schema"`
	Frame       int32                        `json:"frame"`
	Tick        int32                        `json:"tick"`
	RoundState  int32                        `json:"round_state"`
	RandSeed    int32                        `json:"rand_seed"`
	Chars       []libsaturnOracleChar        `json:"chars"`
	Projectiles []libsaturnOracleProjectile  `json:"projectiles"`
}

var (
	libsaturnOracleFile      *os.File
	libsaturnOracleWriter    *bufio.Writer
	libsaturnOracleEncoder   *json.Encoder
	libsaturnOracleFrameNo    int32
	libsaturnOracleMaxFrames  int32 = -1
	libsaturnOracleRoundState int32 = 2
	libsaturnOracleDone       bool
	libsaturnOracleInputs     [][2]uint16
)

func libsaturnOracleEnabled() bool {
	return os.Getenv("LIBSATURN_IKEMEN_ORACLE_TRACE") != ""
}

func libsaturnOracleIntEnv(name string, fallback int32) int32 {
	raw := os.Getenv(name)
	if raw == "" {
		return fallback
	}
	v, err := strconv.ParseInt(raw, 0, 32)
	if err != nil {
		panic(fmt.Sprintf("invalid %s=%q: %v", name, raw, err))
	}
	return int32(v)
}


func libsaturnOracleLoadInputs() error {
	path := os.Getenv("LIBSATURN_IKEMEN_ORACLE_INPUTS")
	if path == "" {
		libsaturnOracleInputs = nil
		return nil
	}
	f, err := os.Open(path)
	if err != nil {
		return err
	}
	defer f.Close()
	libsaturnOracleInputs = nil
	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		var p1, p2 uint16
		if _, err := fmt.Sscanf(scanner.Text(), "%d %d", &p1, &p2); err != nil {
			return fmt.Errorf("invalid oracle input row %q: %w", scanner.Text(), err)
		}
		libsaturnOracleInputs = append(
			libsaturnOracleInputs, [2]uint16{p1, p2})
	}
	return scanner.Err()
}

func libsaturnOracleInput(char *Char, controller int) ([14]bool, bool) {
	var out [14]bool
	if !libsaturnOracleEnabled() || char == nil ||
		sys.roundState() != 2 || controller < 0 || controller > 1 ||
		libsaturnOracleFrameNo < 0 ||
		int(libsaturnOracleFrameNo) >= len(libsaturnOracleInputs) {
		return out, false
	}
	mask := libsaturnOracleInputs[libsaturnOracleFrameNo][controller]
	forward := mask&(1<<0) != 0
	back := mask&(1<<1) != 0
	out[0] = mask&(1<<2) != 0
	out[1] = mask&(1<<3) != 0
	if char.fbFlip {
		out[2] = forward
		out[3] = back
	} else {
		out[2] = back
		out[3] = forward
	}
	out[4] = mask&(1<<4) != 0
	out[5] = mask&(1<<5) != 0
	out[6] = mask&(1<<6) != 0
	out[7] = mask&(1<<7) != 0
	out[8] = mask&(1<<8) != 0
	out[9] = mask&(1<<9) != 0
	out[10] = mask&(1<<10) != 0
	return out, true
}

func libsaturnOracleBeginMatch(s *System) error {
	if !libsaturnOracleEnabled() {
		return nil
	}
	if libsaturnOracleEncoder == nil {
		path := os.Getenv("LIBSATURN_IKEMEN_ORACLE_TRACE")
		f, err := os.Create(path)
		if err != nil {
			return err
		}
		libsaturnOracleFile = f
		libsaturnOracleWriter = bufio.NewWriterSize(f, 256*1024)
		libsaturnOracleEncoder = json.NewEncoder(libsaturnOracleWriter)
		libsaturnOracleEncoder.SetEscapeHTML(false)
		libsaturnOracleMaxFrames =
			libsaturnOracleIntEnv("LIBSATURN_IKEMEN_ORACLE_MAX_FRAMES", -1)
		libsaturnOracleRoundState =
			libsaturnOracleIntEnv("LIBSATURN_IKEMEN_ORACLE_ROUND_STATE", 2)
	}
	s.randseed = libsaturnOracleIntEnv(
		"LIBSATURN_IKEMEN_ORACLE_SEED", 1)
	if err := libsaturnOracleLoadInputs(); err != nil {
		return err
	}
	libsaturnOracleFrameNo = 0
	libsaturnOracleDone = false
	return nil
}

func libsaturnOracleCharSnapshot(c *Char) libsaturnOracleChar {
	animElem := int32(0)
	animTime := int32(0)
	if c.anim != nil {
		animElem = c.anim.curelem + 1
		animTime = c.anim.curtime
	}
	targets := append([]int32(nil), c.targets...)
	hitdefTargets := append([]int32(nil), c.hitdefTargets...)
	return libsaturnOracleChar{
		PlayerNo: c.playerNo,
		HelperIndex: c.helperIndex,
		ID: c.id,
		HelperID: c.helperId,
		ParentID: c.parentId,
		TeamSide: c.teamside,
		StateNo: c.ss.no,
		StateTime: c.ss.time,
		StateType: int32(c.ss.stateType),
		MoveType: int32(c.ss.moveType),
		Ctrl: c.ctrl(),
		Anim: c.animNo,
		AnimElem: animElem,
		AnimTime: animTime,
		Pos: c.pos,
		Vel: c.vel,
		Facing: c.facing,
		Life: c.life,
		Power: c.power,
		Juggle: c.juggle,
		HitPause: c.hitPauseTime,
		MoveContactType: int32(c.mctype),
		MoveContactTime: c.mctime,
		GetHitChainID: c.ghv.chainId(),
		Targets: targets,
		HitDefTargets: hitdefTargets,
	}
}

func libsaturnOracleProjectileSnapshot(
	p *Projectile,
) libsaturnOracleProjectile {
	animElem := int32(0)
	if p.anim != nil {
		animElem = p.anim.curelem + 1
	}
	return libsaturnOracleProjectile{
		PlayerNo: p.playerno,
		OwnerID: p.ownerId,
		ID: p.id,
		Status: int32(p.status),
		Anim: p.animNo,
		AnimElem: animElem,
		Time: p.time,
		Pos: p.pos,
		Vel: p.velocity,
		Facing: p.facing,
		Hits: p.hits,
		MissTime: p.curmisstime,
		HitPause: p.hitpause,
		Contact: p.contactflag,
		Remove: p.remove,
		RemoveTime: p.removetime,
	}
}

func libsaturnOracleCaptureFrame(s *System) bool {
	if libsaturnOracleDone {
		return true
	}
	if libsaturnOracleEncoder == nil {
		return false
	}
	if libsaturnOracleRoundState >= 0 &&
		int32(s.roundState()) != libsaturnOracleRoundState {
		return false
	}

	frame := libsaturnOracleFrame{
		Schema: libsaturnOracleSchema,
		Frame: libsaturnOracleFrameNo,
		Tick: int32(s.tickCount),
		RoundState: int32(s.roundState()),
		RandSeed: s.randseed,
	}

	for playerNo := range s.chars {
		for _, c := range s.chars[playerNo] {
			if c == nil {
				continue
			}
			frame.Chars = append(
				frame.Chars, libsaturnOracleCharSnapshot(c))
		}
	}
	for playerNo := range s.projs {
		for _, p := range s.projs[playerNo] {
			if p == nil {
				continue
			}
			frame.Projectiles = append(
				frame.Projectiles,
				libsaturnOracleProjectileSnapshot(p))
		}
	}

	if err := libsaturnOracleEncoder.Encode(&frame); err != nil {
		panic(err)
	}
	if err := libsaturnOracleWriter.Flush(); err != nil {
		panic(err)
	}

	libsaturnOracleFrameNo++
	if libsaturnOracleMaxFrames >= 0 &&
		libsaturnOracleFrameNo >= libsaturnOracleMaxFrames {
		libsaturnOracleDone = true
		if libsaturnOracleWriter != nil {
			_ = libsaturnOracleWriter.Flush()
		}
		if libsaturnOracleFile != nil {
			_ = libsaturnOracleFile.Close()
			libsaturnOracleFile = nil
		}
		return true
	}
	return false
}
