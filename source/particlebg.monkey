Strict
Import vsat
Import particles


Class ParticleBackground
	
	Field emitter:ParticleEmitter
	
	Method New()
		Local baseUnit:Float = Vsat.ScreenWidth2
		emitter = New ParticleEmitter
		emitter.InitWithSize(100)
		emitter.particleLifeSpan = 12.0
		emitter.particleLifeSpanVariance = 4.0
		emitter.emissionRate = 6
		
		emitter.position.Set(Vsat.ScreenWidth2, Vsat.ScreenHeight * 0.1)
		emitter.positionVariance.Set(baseUnit, baseUnit * 0.5)
		emitter.size.Set(baseUnit/15, baseUnit/15)
		emitter.endSize.Set(baseUnit/12, baseUnit/12)
		emitter.endSizeVariance.Set(baseUnit*0.1, baseUnit*0.1)
		emitter.startColor.Alpha = 0.25
		emitter.endColor.Alpha = 0.0
		emitter.emissionAngle = 90
		emitter.speed = 40
		emitter.speedVariance = 25
		
		emitter.Start()
		emitter.FastForward(20, 0.01666)
	End
	
	Method Update:Void(dt:Float)
		emitter.Update(dt)
	End
	
	Method Render:Void()
		emitter.Render()
	End
	
End
