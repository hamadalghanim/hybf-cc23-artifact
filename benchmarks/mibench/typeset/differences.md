this is the IR before
```llvm
   store %union.rec* %new_par.0, %union.rec** @zz_hold, align 8, !dbg !2017, !tbaa !645
   %opred6636 = getelementptr inbounds %union.rec, %union.rec* %new_par.0, i64 0, i32 0, i32 0, i64 0, i32 0, !dbg !2017
   %1016 = load %union.rec*, %union.rec** %opred6636, align 8, !dbg !2017, !tbaa !647
   store %union.rec* %1016, %union.rec** @zz_tmp, align 8, !dbg !2017, !tbaa !645
   %opred6640 = getelementptr %union.rec, %union.rec* %.in10478, i64 0, i32 0, i32 0, i64 0, i32 0, !dbg !2017
   %1017 = load %union.rec*, %union.rec** %opred6640, align 8, !dbg !2017, !tbaa !647
   %opred6644 = getelementptr %union.rec, %union.rec* %new_par.0, i64 0, i32 0, i32 0, i64 0, i32 0, !dbg !2017
   store %union.rec* %1017, %union.rec** %opred6644, align 8, !dbg !2017, !tbaa !647
```
this is the ir after
```llvm
   %opred6640 = getelementptr %union.rec, %union.rec* %.in10478, i64 0, i32 0, i32 0, i64 0, i32 0, !dbg !2017
   %1016 = load %union.rec*, %union.rec** %opred6640, align 8, !dbg !2017, !tbaa !647
   %opred6644 = getelementptr %union.rec, %union.rec* %new_par.0, i64 0, i32 0, i32 0, i64 0, i32 0, !dbg !2017
   store %union.rec* %1016, %union.rec** %opred6644, align 8, !dbg !2017, !tbaa !647
   store %union.rec* %new_par.0, %union.rec** @zz_hold, align 8, !dbg !2017, !tbaa !645
   %opred6636 = getelementptr inbounds %union.rec, %union.rec* %new_par.0, i64 0, i32 0, i32 0, i64 0, i32 0, !dbg !2017
   %1017 = load %union.rec*, %union.rec** %opred6636, align 8, !dbg !2017, !tbaa !647
   store %union.rec* %1017, %union.rec** @zz_tmp, align 8, !dbg !2017, !tbaa !645
```


```
Basic Block: 'cond.end6663'
	Hash: 857349998300259727
	--------------------
		Tile: 6890042969908013017
		------------
			[0]   %.in10478 = phi %union.rec* [ %call6593, %if.then6592 ], [ %1008, %if.else6594 ]
			[1]   %1011 = getelementptr inbounds %union.rec, %union.rec* %.in10478, i64 0, i32 0, i32 1, i32 0, i32 0, !dbg !2016
			[2]   store i8 0, i8* %1011, align 8, !dbg !2016, !tbaa !647
			[3]   %1012 = getelementptr inbounds %union.rec, %union.rec* %.in10478, i64 0, i32 0, i32 0, i64 1, i32 1, !dbg !2016
			[4]   store %union.rec* %.in10478, %union.rec** %1012, align 8, !dbg !2016, !tbaa !647
			[5]   %1013 = getelementptr inbounds %union.rec, %union.rec* %.in10478, i64 0, i32 0, i32 0, i64 1, i32 0, !dbg !2016
			[6]   store %union.rec* %.in10478, %union.rec** %1013, align 8, !dbg !2016, !tbaa !647
			[7]   %1014 = getelementptr inbounds %union.rec, %union.rec* %.in10478, i64 0, i32 0, i32 0, i64 0, i32 1, !dbg !2016
			[8]   store %union.rec* %.in10478, %union.rec** %1014, align 8, !dbg !2016, !tbaa !647
			[9]   %1015 = getelementptr %union.rec, %union.rec* %.in10478, i64 0, i32 0, i32 0, i64 0, i32 0, !dbg !2016
			[10]   store %union.rec* %.in10478, %union.rec** %1015, align 8, !dbg !2016, !tbaa !647
			[11]   store %union.rec* %.in10478, %union.rec** @xx_link, align 8, !dbg !2016, !tbaa !645
			[12]   store %union.rec* %.in10478, %union.rec** @zz_res, align 8, !dbg !2017, !tbaa !645
			[17]   %opred6640 = getelementptr %union.rec, %union.rec* %.in10478, i64 0, i32 0, i32 0, i64 0, i32 0, !dbg !2017
			[18]   %1017 = load %union.rec*, %union.rec** %opred6640, align 8, !dbg !2017, !tbaa !647
			[19]   %opred6644 = getelementptr %union.rec, %union.rec* %new_par.0, i64 0, i32 0, i32 0, i64 0, i32 0, !dbg !2017
			[20]   store %union.rec* %1017, %union.rec** %opred6644, align 8, !dbg !2017, !tbaa !647

		Tile:
		Hash: 1891494345358921040
		------------
			[13]   store %union.rec* %new_par.0, %union.rec** @zz_hold, align 8, !dbg !2017, !tbaa !645

		Tile:
		Hash: 16529784631404651171
		------------
			[14]   %opred6636 = getelementptr inbounds %union.rec, %union.rec* %new_par.0, i64 0, i32 0, i32 0, i64 0, i32 0, !dbg !2017
			[15]   %1016 = load %union.rec*, %union.rec** %opred6636, align 8, !dbg !2017, !tbaa !647
			[16]   store %union.rec* %1016, %union.rec** @zz_tmp, align 8, !dbg !2017, !tbaa !645

		Tile:
		Hash: 3725305709271303173
		------------
			[21]   %1018 = load %union.rec*, %union.rec** @zz_hold, align 8, !dbg !2017, !tbaa !645
			[22]   %1019 = load %struct.word_type**, %struct.word_type*** bitcast (%union.rec** @zz_res to %struct.word_type***), align 8, !dbg !2017, !tbaa !645
			[23]   %1020 = load %struct.word_type*, %struct.word_type** %1019, align 8, !dbg !2017, !tbaa !647
			[24]   %osucc6652 = getelementptr inbounds %struct.word_type, %struct.word_type* %1020, i64 0, i32 0, i64 0, i32 1, !dbg !2017
			[25]   store %union.rec* %1018, %union.rec** %osucc6652, align 8, !dbg !2017, !tbaa !647
			[26]   %1021 = load %union.rec*, %union.rec** @zz_tmp, align 8, !dbg !2017, !tbaa !645
			[27]   %opred6656 = bitcast %struct.word_type** %1019 to %union.rec**, !dbg !2017
			[28]   store %union.rec* %1021, %union.rec** %opred6656, align 8, !dbg !2017, !tbaa !647

		Tile:
		Hash: 7181069308620291598
		------------
			[29]   %1022 = load %union.rec*, %union.rec** @zz_res, align 8, !dbg !2017, !tbaa !645
			[30]   %1023 = load %struct.word_type*, %struct.word_type** bitcast (%union.rec** @zz_tmp to %struct.word_type**), align 8, !dbg !2017, !tbaa !645
			[31]   %osucc6660 = getelementptr inbounds %struct.word_type, %struct.word_type* %1023, i64 0, i32 0, i64 0, i32 1, !dbg !2017
			[32]   store %union.rec* %1022, %union.rec** %osucc6660, align 8, !dbg !2017, !tbaa !647

		Tile:
		Hash: 2350060225857077334
		------------
			[33]   %.pre10444 = load %union.rec*, %union.rec** @xx_link, align 8, !dbg !2017, !tbaa !645
			[34]   store %union.rec* %.pre10444, %union.rec** @zz_res, align 8, !dbg !2017, !tbaa !645
			[36]   %cmp6665 = icmp eq %union.rec* %call6576, null, !dbg !2017
			[37]   %cmp6669 = icmp eq %union.rec* %.pre10444, null, !dbg !2017
			[38]   %or.cond10174 = select i1 %cmp6665, i1 true, i1 %cmp6669, !dbg !2017

		Tile:
		Hash: 2936600829885129667
		------------
			[35]   store %union.rec* %call6576, %union.rec** @zz_hold, align 8, !dbg !2017, !tbaa !645
```


# ASM

```diff
--- a/baseline.S (original)
+++ b/TFG.S (current)
@@ -1,1 +1,1 @@
-  4123ee:	4c 89 35 93 70 08 00 	mov    %r14,0x87093(%rip)        # 499488 <zz_hold>
-  4123f5:	49 8b 0e             	mov    (%r14),%rcx
-  4123f8:	48 89 0d 91 70 08 00 	mov    %rcx,0x87091(%rip)        # 499490 <zz_tmp>
-  4123ff:	48 8b 00             	mov    (%rax),%rax
-  412402:	49 89 06             	mov    %rax,(%r14)
-  412405:	48 8b 05 7c 70 08 00 	mov    0x8707c(%rip),%rax        # 499488 <zz_hold>
-  41240c:	48 8b 0d 85 70 08 00 	mov    0x87085(%rip),%rcx        # 499498 <zz_res>
-  412413:	48 8b 11             	mov    (%rcx),%rdx
-  412416:	48 89 42 08          	mov    %rax,0x8(%rdx)
-  41241a:	48 8b 05 6f 70 08 00 	mov    0x8706f(%rip),%rax        # 499490 <zz_tmp>
-  412421:	48 89 01             	mov    %rax,(%rcx)
-  412424:	48 8b 05 6d 70 08 00 	mov    0x8706d(%rip),%rax        # 499498 <zz_res>
-  41242b:	48 8b 0d 5e 70 08 00 	mov    0x8705e(%rip),%rcx        # 499490 <zz_tmp>
-  412432:	48 89 41 08          	mov    %rax,0x8(%rcx)
-  412436:	48 8b 05 6b 70 08 00 	mov    0x8706b(%rip),%rax        # 4994a8 <xx_link>
-  41243d:	48 89 05 54 70 08 00 	mov    %rax,0x87054(%rip)        # 499498 <zz_res>
-  412444:	48 89 1d 3d 70 08 00 	mov    %rbx,0x8703d(%rip)        # 499488 <zz_hold>
-  41244b:	48 85 db             	test   %rbx,%rbx
-  41244e:	74 28                	je     412478 <Parse+0x3018>
-  412450:	48 85 c0             	test   %rax,%rax
-  412453:	74 23                	je     412478 <Parse+0x3018>
-  412455:	48 8b 4b 10          	mov    0x10(%rbx),%rcx
-  412459:	48 89 0d 30 70 08 00 	mov    %rcx,0x87030(%rip)        # 499490 <zz_tmp>
-  412460:	48 8b 50 10          	mov    0x10(%rax),%rdx
-  412464:	48 89 53 10          	mov    %rdx,0x10(%rbx)
-  412468:	48 8b 50 10          	mov    0x10(%rax),%rdx
-  41246c:	48 89 5a 18          	mov    %rbx,0x18(%rdx)
-  412470:	48 89 48 10          	mov    %rcx,0x10(%rax)
-  412474:	48 89 41 18          	mov    %rax,0x18(%rcx)
-  412478:	85 ed                	test   %ebp,%ebp
-  41247a:	7e 0e                	jle    41248a <Parse+0x302a>
-  41247c:	89 eb                	mov    %ebp,%ebx
-  41247e:	66 90                	xchg   %ax,%ax


+  4123ee:	48 8b 00             	mov    (%rax),%rax
+  4123f1:	49 89 06             	mov    %rax,(%r14)
+  4123f4:	4c 89 35 8d 70 08 00 	mov    %r14,0x8708d(%rip)        # 499488 <zz_hold>
+  4123fb:	49 8b 06             	mov    (%r14),%rax
+  4123fe:	48 89 05 8b 70 08 00 	mov    %rax,0x8708b(%rip)        # 499490 <zz_tmp>
+  412405:	48 8b 0d 8c 70 08 00 	mov    0x8708c(%rip),%rcx        # 499498 <zz_res>
+  41240c:	48 8b 11             	mov    (%rcx),%rdx
+  41240f:	4c 89 72 08          	mov    %r14,0x8(%rdx)
+  412413:	48 89 01             	mov    %rax,(%rcx)
+  412416:	48 8b 05 7b 70 08 00 	mov    0x8707b(%rip),%rax        # 499498 <zz_res>
+  41241d:	48 8b 0d 6c 70 08 00 	mov    0x8706c(%rip),%rcx        # 499490 <zz_tmp>
+  412424:	48 89 41 08          	mov    %rax,0x8(%rcx)
+  412428:	48 8b 05 79 70 08 00 	mov    0x87079(%rip),%rax        # 4994a8 <xx_link>
+  41242f:	48 89 05 62 70 08 00 	mov    %rax,0x87062(%rip)        # 499498 <zz_res>
+  412436:	48 89 1d 4b 70 08 00 	mov    %rbx,0x8704b(%rip)        # 499488 <zz_hold>
+  41243d:	48 85 db             	test   %rbx,%rbx
+  412440:	74 28                	je     41246a <Parse+0x300a>
+  412442:	48 85 c0             	test   %rax,%rax
+  412445:	74 23                	je     41246a <Parse+0x300a>
+  412447:	48 8b 4b 10          	mov    0x10(%rbx),%rcx
+  41244b:	48 89 0d 3e 70 08 00 	mov    %rcx,0x8703e(%rip)        # 499490 <zz_tmp>
+  412452:	48 8b 50 10          	mov    0x10(%rax),%rdx
+  412456:	48 89 53 10          	mov    %rdx,0x10(%rbx)
+  41245a:	48 8b 50 10          	mov    0x10(%rax),%rdx
+  41245e:	48 89 5a 18          	mov    %rbx,0x18(%rdx)
+  412462:	48 89 48 10          	mov    %rcx,0x10(%rax)
+  412466:	48 89 41 18          	mov    %rax,0x18(%rcx)
+  41246a:	85 ed                	test   %ebp,%ebp
+  41246c:	7e 0c                	jle    41247a <Parse+0x301a>
+  41246e:	89 eb                	mov    %ebp,%ebx
```