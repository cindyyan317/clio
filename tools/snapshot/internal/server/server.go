package server

import (
	"log"
	"net"

	pb "xrplf/clio/clio_snapshot/org/xrpl/rpc/v1"

	"google.golang.org/grpc"
)

func StartServer(grpcServerAddr string, wsServerAddr string) {
	lis, err := net.Listen("tcp", grpcServerAddr)

	if err != nil {
		log.Fatalf("Failed to listen: %v", err)
	}

	grpcServer := grpc.NewServer()
	pb.RegisterXRPLedgerAPIServiceServer(grpcServer, newServer())
	log.Print("Starting server...")
	go grpcServer.Serve(lis)
	wsServer := NewWebSocketServer("Snapshot Server", func(message string) string {
		return "Received: " + message
	})
	wsServer.Start(wsServerAddr)

	select {}
}
